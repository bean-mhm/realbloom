#include "CMImageIO.h"

bool CMImageIO::readImage(CMImage& target, const std::string& filename, const std::string& colorSpace, std::string& outError)
{
    outError = "";

    // Open the file
    OIIO::ImageInput::unique_ptr inp = OIIO::ImageInput::open(filename);
    if (!inp)
    {
        outError = stringFormat("Couldn't open input file \"%s\".", filename.c_str());
        return false;
    }

    // Read the specs
    const OIIO::ImageSpec& spec = inp->spec();
    int xres = spec.width;
    int yres = spec.height;
    int channels = spec.nchannels;

    if ((channels != 1) && (channels != 3) && (channels != 4))
    {
        outError = stringFormat("Image must have 1, 3, or 4 color channels. (%d)", channels);
        inp->close();
        return false;
    }

    // Prepare buffer
    std::vector<float> buffer;
    buffer.resize(xres * yres * channels);

    // Read into the buffer
    if (!inp->read_image(0, 0, 0, -1, OIIO::TypeDesc::FLOAT, buffer.data()))
    {
        outError = stringFormat("Couldn't read image from file \"%s\".", filename.c_str());
        inp->close();
        return false;
    }
    inp->close();

    // Convert to RGBA
    std::vector<float> bufferRGBA;
    bufferRGBA.resize(xres * yres * 4);
    if (channels == 4)
        std::copy(buffer.data(), buffer.data() + buffer.size(), bufferRGBA.data());
    else if (channels == 3)
    {
        uint32_t redIndexSource, redIndexTarget;
        for (uint32_t y = 0; y < yres; y++)
        {
            for (uint32_t x = 0; x < xres; x++)
            {
                redIndexSource = (y * xres + x) * 3;
                redIndexTarget = (y * xres + x) * 4;
                bufferRGBA[redIndexTarget + 0] = buffer[redIndexSource + 0];
                bufferRGBA[redIndexTarget + 1] = buffer[redIndexSource + 1];
                bufferRGBA[redIndexTarget + 2] = buffer[redIndexSource + 2];
                bufferRGBA[redIndexTarget + 3] = 1.0f;
            }
        }
    } else if (channels == 1)
    {
        float v;
        uint32_t redIndexSource, redIndexTarget;
        for (uint32_t y = 0; y < yres; y++)
        {
            for (uint32_t x = 0; x < xres; x++)
            {
                redIndexSource = y * xres + x;
                redIndexTarget = (y * xres + x) * 4;

                v = buffer[redIndexSource];
                bufferRGBA[redIndexTarget + 0] = v;
                bufferRGBA[redIndexTarget + 1] = v;
                bufferRGBA[redIndexTarget + 2] = v;
                bufferRGBA[redIndexTarget + 3] = 1.0f;
            }
        }
    }

    // Color Space Conversion
    if (channels > 1)
    {
        try
        {
            OCIO::ConstConfigRcPtr config = CMS::getConfig();
            OCIO::PackedImageDesc img(
                bufferRGBA.data(),
                xres,
                yres,
                OCIO::ChannelOrdering::CHANNEL_ORDERING_RGBA,
                OCIO::BitDepth::BIT_DEPTH_F32,
                4,                 // 4 bytes to go to the next color channel
                4 * 4,             // 4 color channels * 4 bytes per channel
                xres * 4 * 4);     // width * 4 channels * 4 bytes

            OCIO::ColorSpaceTransformRcPtr transform = OCIO::ColorSpaceTransform::Create();
            transform->setSrc(colorSpace.c_str());
            transform->setDst(OCIO::ROLE_SCENE_LINEAR);

            OCIO::ConstProcessorRcPtr proc = config->getProcessor(transform);
            OCIO::ConstCPUProcessorRcPtr cpuProc = proc->getDefaultCPUProcessor();
            cpuProc->apply(img);
        } catch (OCIO::Exception& exception)
        {
            outError = stringFormat("OpenColorIO Error: %s", exception.what());
            return false;
        }
    }

    // Move buffer to the target image
    {
        std::lock_guard<CMImage> lock(target);
        target.resize(xres, yres, false);
        float* targetBuffer = target.getImageData();
        std::copy(bufferRGBA.data(), bufferRGBA.data() + bufferRGBA.size(), targetBuffer);
    }
    target.moveToGPU();

    return true;
}

bool CMImageIO::writeImage(CMImage& source, const std::string& filename, const std::string& colorSpace, std::string& outError)
{
    outError = "";

    // Grab image buffer (RGBA)
    source.lock();
    int xres = source.getWidth();
    int yres = source.getHeight();
    float* sourceBuffer = source.getImageData();

    // Prepare RGB buffer
    std::vector<float> bufferRGB;
    bufferRGB.resize(xres * yres * 3);

    // Copy to RGB buffer
    uint32_t redIndexSource, redIndexTarget;
    for (uint32_t y = 0; y < yres; y++)
    {
        for (uint32_t x = 0; x < xres; x++)
        {
            redIndexSource = (y * xres + x) * 4;
            redIndexTarget = (y * xres + x) * 3;
            bufferRGB[redIndexTarget + 0] = sourceBuffer[redIndexSource + 0];
            bufferRGB[redIndexTarget + 1] = sourceBuffer[redIndexSource + 1];
            bufferRGB[redIndexTarget + 2] = sourceBuffer[redIndexSource + 2];
        }
    }
    source.unlock();

    // Color Space Conversion
    try
    {
        OCIO::ConstConfigRcPtr config = CMS::getConfig();
        OCIO::PackedImageDesc img(
            bufferRGB.data(),
            xres,
            yres,
            OCIO::ChannelOrdering::CHANNEL_ORDERING_RGB,
            OCIO::BitDepth::BIT_DEPTH_F32,
            4,                 // 4 bytes to go to the next color channel
            3 * 4,             // 3 color channels * 4 bytes per channel
            xres * 3 * 4);     // width * 3 channels * 4 bytes

        OCIO::ColorSpaceTransformRcPtr transform = OCIO::ColorSpaceTransform::Create();
        transform->setSrc(OCIO::ROLE_SCENE_LINEAR);
        transform->setDst(colorSpace.c_str());

        OCIO::ConstProcessorRcPtr proc = config->getProcessor(transform);
        OCIO::ConstCPUProcessorRcPtr cpuProc = proc->getDefaultCPUProcessor();
        cpuProc->apply(img);
    } catch (OCIO::Exception& exception)
    {
        outError = stringFormat("OpenColorIO Error: %s", exception.what());
        return false;
    }

    // Create output file
    std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create("openexr");
    if (!out)
    {
        outError = "Couldn't create ImageOutput.";
        return false;
    }

    // Write output
    OIIO::ImageSpec spec(xres, yres, 3, OIIO::TypeDesc::FLOAT);
    if (out->open(filename, spec))
    {
        if (out->write_image(OIIO::TypeDesc::FLOAT, bufferRGB.data()))
            out->close();
        else
        {
            outError = stringFormat("Couldn't write image to file \"%s\".", filename.c_str());
            out->close();
            return false;
        }
    } else
    {
        outError = stringFormat("Couldn't open output file \"%s\".", filename.c_str());
        return false;
    }

    return true;
}