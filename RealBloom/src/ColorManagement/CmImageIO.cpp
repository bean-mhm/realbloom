#include "CmImageIO.h"

CmImageIO::CmImageIoVars* CmImageIO::S_VARS = nullptr;

static const std::string attribNameColorSpace = "colorspace";

bool CmImageIO::init()
{
    S_VARS = new CmImageIoVars();

    S_VARS->inputSpace = CMS::getWorkingSpace();
    S_VARS->outputSpace = S_VARS->inputSpace;
    S_VARS->nonLinearSpace = S_VARS->inputSpace;

    const std::vector<std::string>& userSpaces = CMS::getColorSpaces();

    for (const auto& space : userSpaces)
    {
        if (strLowercase(space).starts_with("srgb"))
        {
            S_VARS->nonLinearSpace = space;
            break;
        }
    }
}

void CmImageIO::cleanUp()
{
    DELPTR(S_VARS);
}

const std::string& CmImageIO::getInputSpace()
{
    return S_VARS->inputSpace;
}

const std::string& CmImageIO::getOutputSpace()
{
    return S_VARS->outputSpace;
}

const std::string& CmImageIO::getNonLinearSpace()
{
    return S_VARS->nonLinearSpace;
}

bool CmImageIO::getAutoDetect()
{
    return S_VARS->autoDetect;
}

bool CmImageIO::getApplyViewTransform()
{
    return S_VARS->applyViewTransform;
}

void CmImageIO::setInputSpace(const std::string& colorSpace)
{
    S_VARS->inputSpace = colorSpace;
}

void CmImageIO::setOutputSpace(const std::string& colorSpace)
{
    S_VARS->outputSpace = colorSpace;
}

void CmImageIO::setNonLinearSpace(const std::string& colorSpace)
{
    S_VARS->nonLinearSpace = colorSpace;
}

void CmImageIO::setAutoDetect(bool autoDetect)
{
    S_VARS->autoDetect = autoDetect;
}

void CmImageIO::setApplyViewTransform(bool applyViewTransform)
{
    S_VARS->applyViewTransform = applyViewTransform;
}

std::string makeIoError(const std::string& message, bool hasError, const std::string& error)
{
    if (hasError)
        return strFormat("%s: %s", message.c_str(), error.c_str());
    else
        return strFormat("%s.", message.c_str());
}

void CmImageIO::readImage(CmImage& target, const std::string& filename)
{
    try
    {
        if (filename.empty())
            throw std::exception("Filename was empty.");

        // Get the extension
        std::string extension = getFileExtension(filename);

        // Determine the color space
        std::string csName;
        if (contains(getLinearExtensions(), extension))
        {
            csName = CMS::resolveColorSpace(S_VARS->inputSpace);
        }
        else if (contains(getNonLinearExtensions(), extension))
        {
            csName = CMS::resolveColorSpace(S_VARS->nonLinearSpace);
        }
        else
        {
            throw std::exception(strFormat("File extension \"%s\" isn't supported.", extension.c_str()).c_str());
        }

        // Open the file
        OIIO::ImageInput::unique_ptr inp = OIIO::ImageInput::open(filename);
        if (!inp)
        {
            throw std::exception(strFormat("Couldn't open input file \"%s\".", filename.c_str()).c_str());
        }

        // Read the specs
        const OIIO::ImageSpec& spec = inp->spec();
        uint32_t width = spec.width;
        uint32_t height = spec.height;
        uint32_t channels = spec.nchannels;

        // Attempt to read the color space name
        if (S_VARS->autoDetect && contains(getMetaExtensions(), extension))
        {
            std::string attribColorSpace = spec.get_string_attribute(attribNameColorSpace, "");
            if (!attribColorSpace.empty())
            {
                std::string resolvedColorSpace = CMS::resolveColorSpace(attribColorSpace, false);
                if (resolvedColorSpace.empty())
                {
                    printWarning(__FUNCTION__, "Auto-Detect", strFormat("The read color space \"%s\" wasn't found.", attribColorSpace.c_str()));
                }
                else
                {
                    csName = resolvedColorSpace;
                    printInfo(__FUNCTION__, "Auto-Detect", strFormat("Detected color space \"%s\".", resolvedColorSpace.c_str()));
                }
            }
        }

        if ((channels != 1) && (channels != 3) && (channels != 4))
        {
            inp->close();
            throw std::exception(strFormat("Input image must have 1, 3, or 4 color channels, not %d.", channels).c_str());
        }

        // Prepare buffer
        std::vector<float> buffer;
        buffer.resize(width * height * channels);

        // Read into the buffer
        if (!inp->read_image(0, 0, 0, -1, OIIO::TypeDesc::FLOAT, buffer.data()))
        {
            inp->close();
            throw std::exception(makeIoError(
                strFormat("Couldn't read image from file \"%s\"", filename.c_str()),
                inp->has_error(),
                inp->geterror()
            ).c_str());
        }
        inp->close();

        // Convert to RGBA
        std::vector<float> bufferRGBA;
        bufferRGBA.resize(width * height * 4);
        if (channels == 4)
            std::copy(buffer.data(), buffer.data() + buffer.size(), bufferRGBA.data());
        else if (channels == 3)
        {
            uint32_t redIndexSource, redIndexTarget;
            for (uint32_t y = 0; y < height; y++)
            {
                for (uint32_t x = 0; x < width; x++)
                {
                    redIndexSource = (y * width + x) * 3;
                    redIndexTarget = (y * width + x) * 4;
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
            for (uint32_t y = 0; y < height; y++)
            {
                for (uint32_t x = 0; x < width; x++)
                {
                    redIndexSource = y * width + x;
                    redIndexTarget = (y * width + x) * 4;

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
            OCIO::ConstConfigRcPtr config = CMS::getConfig();

            try
            {
                OCIO::PackedImageDesc img(
                    bufferRGBA.data(),
                    width,
                    height,
                    OCIO::ChannelOrdering::CHANNEL_ORDERING_RGBA,
                    OCIO::BitDepth::BIT_DEPTH_F32,
                    4,                 // 4 bytes to go to the next color channel
                    4 * 4,             // 4 color channels * 4 bytes per channel
                    width * 4 * 4);     // width * 4 channels * 4 bytes

                OCIO::ColorSpaceTransformRcPtr transform = OCIO::ColorSpaceTransform::Create();
                transform->setSrc(csName.c_str());
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
            std::scoped_lock lock(target);
            target.resize(width, height, false);
            float* targetBuffer = target.getImageData();
            std::copy(bufferRGBA.data(), bufferRGBA.data() + bufferRGBA.size(), targetBuffer);
        }
        target.moveToGPU();

        // Update target source name
        target.setSourceName(std::filesystem::path(filename).filename().string());
    }
    catch (const std::exception& e)
    {
        throw std::exception(makeError(__FUNCTION__, "", e.what()).c_str());
    }
}

void CmImageIO::writeImage(CmImage& source, const std::string& filename)
{
    try
    {
        if (filename.empty())
            throw std::exception("Filename was empty.");

        // Get the extension

        std::string extension = getFileExtension(filename);

        if (!contains(getAllExtensions(), extension))
            throw std::exception(strFormat("File extension \"%s\" isn't supported.", extension.c_str()).c_str());

        bool nonLinear = contains(getNonLinearExtensions(), extension);

        // Grab the image buffer
        source.lock();
        uint32_t width = source.getWidth();
        uint32_t height = source.getHeight();
        float* sourceBuffer = source.getImageData();
        uint32_t sourceBufferSize = source.getImageDataSize();

        // Copy the buffer
        std::vector<float> buffer;
        buffer.resize(sourceBufferSize);
        std::copy(sourceBuffer, sourceBuffer + sourceBufferSize, buffer.data());

        // Release the image
        source.unlock();

        // Get the OCIO config
        CMS::ensureOK();
        OCIO::ConstConfigRcPtr config = CMS::getConfig();

        // Resolve the output color space name
        std::string csName = CMS::resolveColorSpace(S_VARS->outputSpace);

        bool viewTransform = S_VARS->applyViewTransform || nonLinear;

        // Color transform
        if (viewTransform)
        {
            // Apply view transform
            std::shared_ptr<GlTexture> texture = nullptr;
            std::shared_ptr<GlFrameBuffer> frameBuffer = nullptr;
            CmImage::applyViewTransform(
                buffer.data(),
                width,
                height,
                CMS::getExposure(),
                texture,
                frameBuffer,
                true,
                true,
                true,
                &buffer);
        }
        else
        {
            // Color space conversion
            try
            {
                OCIO::PackedImageDesc img(
                    buffer.data(),
                    width,
                    height,
                    OCIO::ChannelOrdering::CHANNEL_ORDERING_RGBA,
                    OCIO::BitDepth::BIT_DEPTH_F32,
                    4,                 // 4 bytes to go to the next color channel
                    4 * 4,             // 4 color channels * 4 bytes per channel
                    width * 4 * 4);     // width * 4 channels * 4 bytes

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
        }

        // Create output directories if necessary
        std::filesystem::create_directories(std::filesystem::path(filename).parent_path());

        // Create ImageOutput
        std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create(filename);
        if (!out)
        {
            throw std::exception("Couldn't create ImageOutput.");
        }

        // Create ImageSpec
        OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::FLOAT);

        // Embed the color space name
        if ((!viewTransform) && contains(getMetaExtensions(), extension))
        {
            spec.attribute(attribNameColorSpace, OIIO::TypeDesc::TypeString, csName);
        }

        // Write the image
        if (out->open(filename, spec))
        {
            if (out->write_image(OIIO::TypeDesc::FLOAT, buffer.data()))
                out->close();
            else
            {
                out->close();
                throw std::exception(makeIoError(
                    strFormat("Couldn't write image to file \"%s\"", filename.c_str()),
                    out->has_error(),
                    out->geterror()
                ).c_str());
            }
        }
        else
        {
            throw std::exception(makeIoError(
                strFormat("Couldn't open output file \"%s\"", filename.c_str()),
                out->has_error(),
                out->geterror()
            ).c_str());
        }

        // Update the target's source name
        source.setSourceName(std::filesystem::path(filename).filename().string());
    }
    catch (const std::exception& e)
    {
        throw std::exception(makeError(__FUNCTION__, "", e.what()).c_str());
    }
}

const std::vector<std::string>& CmImageIO::getLinearExtensions()
{
    static std::vector<std::string> exts
    {
        ".exr",
        ".tif",
        ".tiff"
    };
    return exts;
}

const std::vector<std::string>& CmImageIO::getNonLinearExtensions()
{
    static std::vector<std::string> exts
    {
        ".png",
        ".jpg",
        ".jpeg",
        ".bmp"
    };
    return exts;
}

const std::vector<std::string>& CmImageIO::getAllExtensions()
{
    static std::vector<std::string> exts;
    static bool init = false;

    if (!init)
    {
        init = true;

        const std::vector<std::string>& linearExts = getLinearExtensions();
        const std::vector<std::string>& nonLinearExts = getNonLinearExtensions();

        exts.reserve(linearExts.size() + nonLinearExts.size());
        exts.insert(exts.end(), linearExts.begin(), linearExts.end());
        exts.insert(exts.end(), nonLinearExts.begin(), nonLinearExts.end());
    }

    return exts;
}

const std::vector<std::string>& CmImageIO::getMetaExtensions()
{
    static std::vector<std::string> exts
    {
        ".exr"
    };
    return exts;
}

const std::vector<std::string>& CmImageIO::getOpenFilterList()
{
    static std::vector<std::string> filterList
    {
        "All Images",
        "exr,tif,tiff,png,jpg,jpeg,bmp",

        "Linear",
        "exr,tif,tiff",

        "Non-Linear",
        "png,jpg,jpeg,bmp"
    };
    return filterList;
}

const std::vector<std::string>& CmImageIO::getSaveFilterList()
{
    static std::vector<std::string> filterList
    {
        "OpenEXR",
        "exr",

        "TIFF",
        "tif,tiff",

        "PNG",
        "png",

        "JPEG",
        "jpg,jpeg",

        "Bitmap",
        "bmp"
    };
    return filterList;
}

std::string CmImageIO::getDefaultFilename()
{
    return "untitled";
}
