#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <filesystem>

#include "oiio/imageio.h"

#include "ocio/OpenColorIO.h"
namespace OCIO = OpenColorIO_v2_1;

#include "CMS.h"
#include "CmImage.h"

#include "../Utils/OpenGL/GlTexture.h"
#include "../Utils/OpenGL/GlFrameBuffer.h"
#include "../Utils/OpenGL/GlUtils.h"
#include "../Utils/Misc.h"

class CmImageIO
{
private:
    struct CmImageIoVars
    {
        std::string inputSpace = "";
        std::string outputSpace = "";
        std::string nonLinearSpace = "";
        bool autoDetect = true;
        bool applyViewTransform = false;
    };
    static CmImageIoVars* S_VARS;

public:
    CmImageIO() = delete;
    CmImageIO(const CmImageIO&) = delete;
    CmImageIO& operator= (const CmImageIO&) = delete;

    static bool init();
    static void cleanUp();

    static const std::string& getInputSpace();
    static const std::string& getOutputSpace();
    static const std::string& getNonLinearSpace();
    static bool getAutoDetect();
    static bool getApplyViewTransform();

    static void setInputSpace(const std::string& colorSpace);
    static void setOutputSpace(const std::string& colorSpace);
    static void setNonLinearSpace(const std::string& colorSpace);
    static void setAutoDetect(bool autoDetect);
    static void setApplyViewTransform(bool applyViewTransform);

    static void readImage(CmImage& target, const std::string& filename);
    static void writeImage(CmImage& source, const std::string& filename);

    static const std::vector<std::string>& getLinearExtensions();
    static const std::vector<std::string>& getNonLinearExtensions();
    static const std::vector<std::string>& getAllExtensions();

    // Linear extensions that support custom metadata
    static const std::vector<std::string>& getMetaExtensions();

    static const std::vector<std::string>& getOpenFilterList();
    static const std::vector<std::string>& getSaveFilterList();
    static std::string getDefaultFilename();

};
