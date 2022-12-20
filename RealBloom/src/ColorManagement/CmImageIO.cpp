#include "CmImageIO.h"

bool CmImageIO::readImageColorSpace(const std::string& filename, std::string& outColorSpace)
{
    outColorSpace = "";

    std::filesystem::path path(filename);
    std::string extension = strLowercase(path.extension().string());

    OCIO::ConstConfigRcPtr config = CMS::getConfig();
    const std::vector<std::string>& userSpaces = CMS::getAvailableColorSpaces();

    // return sRGB for PNG and JPEG formats
    if ((extension == ".png") || (extension == ".jpg"))
    {
        for (const auto& space : userSpaces)
        {
            if (space.starts_with("sRGB "))
            {
                outColorSpace = space;
                return true;
            }
        }
        return false;
    }

    try
    {
        // Open the file
        OIIO::ImageInput::unique_ptr inp = OIIO::ImageInput::open(filename);
        if (!inp)
            return false;

        // Read the color space
        const OIIO::ImageSpec& spec = inp->spec();
        std::string attribColorSpace = spec.get_string_attribute("colorspace", "");
        std::string attribOiioSpace = spec.get_string_attribute("oiio:ColorSpace", "");
        inp->close();

        OCIO::ConstColorSpaceRcPtr colorSpace = config->getColorSpace(attribColorSpace.c_str());
        OCIO::ConstColorSpaceRcPtr oiioSpace = config->getColorSpace(attribOiioSpace.c_str());

        // If the color space exists in the user config, return it
        if (colorSpace.get() != nullptr)
        {
            outColorSpace = attribColorSpace;
            return true;
        }
        else if (oiioSpace.get() != nullptr)
        {
            outColorSpace = attribOiioSpace;
            return true;
        }
    }
    catch (const std::exception& e)
    {
        printErr(__FUNCTION__, e.what());
    }

    return false;
}

void CmImageIO::readImage(CmImage& target, const std::string& filename, const std::string& colorSpace)
{
    try
    {
        if (filename.empty())
            throw std::exception("Filename was empty.");

        // Open the file
        OIIO::ImageInput::unique_ptr inp = OIIO::ImageInput::open(filename);
        if (!inp)
            throw std::exception(strFormat("Couldn't open input file \"%s\".", filename.c_str()).c_str());

        // Read the specs
        const OIIO::ImageSpec& spec = inp->spec();
        int xres = spec.width;
        int yres = spec.height;
        int channels = spec.nchannels;

        if ((channels != 1) && (channels != 3) && (channels != 4))
        {
            inp->close();
            throw std::exception(strFormat("Image must have 1, 3, or 4 color channels. (%d)", channels).c_str());
        }

        // Prepare buffer
        std::vector<float> buffer;
        buffer.resize(xres * yres * channels);

        // Read into the buffer
        if (!inp->read_image(0, 0, 0, -1, OIIO::TypeDesc::FLOAT, buffer.data()))
        {
            inp->close();
            throw std::exception(strFormat("Couldn't read image from file \"%s\".", filename.c_str()).c_str());
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
        }
        else if (channels == 1)
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
            CMS::ensureOK();
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
            }
            catch (OCIO::Exception& e)
            {
                throw std::exception(strFormat("OpenColorIO Error: %s", e.what()).c_str());
            }
        }

        // Move buffer to the target image
        {
            std::lock_guard<CmImage> lock(target);
            target.resize(xres, yres, false);
            float* targetBuffer = target.getImageData();
            std::copy(bufferRGBA.data(), bufferRGBA.data() + bufferRGBA.size(), targetBuffer);
        }
        target.moveToGPU();

        // Update target source name
        target.setSourceName(std::filesystem::path(filename).filename().string());
    }
    catch (const std::exception& e)
    {
        throw std::exception(printErr(__FUNCTION__, e.what()).c_str());
    }
}

void CmImageIO::writeImage(CmImage& source, const std::string& filename, const std::string& colorSpace)
{
    try
    {
        if (filename.empty())
            throw std::exception("Filename was empty.");

        if (strLowercase(std::filesystem::path(filename).extension().string()) != ".exr")
            throw std::exception("Output extension must be OpenEXR (.exr).");

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

        // Get the OCIO config
        CMS::ensureOK();
        OCIO::ConstConfigRcPtr config = CMS::getConfig();

        // Resolve color space name
        OCIO::ConstColorSpaceRcPtr cs = config->getColorSpace(colorSpace.c_str());
        if (cs.get() == nullptr)
            throw std::exception(strFormat("Color space \"%s\" was not found.", colorSpace.c_str()).c_str());
        std::string csName = cs->getName();

        // Color space conversion
        try
        {
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
            transform->setDst(csName.c_str());

            OCIO::ConstProcessorRcPtr proc = config->getProcessor(transform);
            OCIO::ConstCPUProcessorRcPtr cpuProc = proc->getDefaultCPUProcessor();
            cpuProc->apply(img);
        }
        catch (OCIO::Exception& e)
        {
            throw std::exception(strFormat("OpenColorIO Error: %s", e.what()).c_str());
        }

        // Create output directories if necessary
        std::filesystem::create_directories(std::filesystem::path(filename).parent_path());

        // Create ImageOutput
        std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create("openexr");
        if (!out)
        {
            throw std::exception("Couldn't create ImageOutput.");
        }

        // Write output
        OIIO::ImageSpec spec(xres, yres, 3, OIIO::TypeDesc::FLOAT);
        spec.attribute("oiio:ColorSpace", OIIO::TypeDesc::TypeString, csName);
        spec.attribute("colorspace", OIIO::TypeDesc::TypeString, csName);
        if (out->open(filename, spec))
        {
            if (out->write_image(OIIO::TypeDesc::FLOAT, bufferRGB.data()))
                out->close();
            else
            {
                out->close();
                throw std::exception(strFormat("Couldn't write image to file \"%s\".", filename.c_str()).c_str());
            }
        }
        else
        {
            throw std::exception(strFormat("Couldn't open output file \"%s\".", filename.c_str()).c_str());
        }

        // Update target source name
        source.setSourceName(std::filesystem::path(filename).filename().string());
    }
    catch (const std::exception& e)
    {
        throw std::exception(printErr(__FUNCTION__, e.what()).c_str());
    }
}
