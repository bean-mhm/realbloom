#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <filesystem>

#include "OpenImageIO/imageio.h"

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

#include "CMS.h"
#include "CMImage.h"

#include "../Utils/Misc.h"

class CmImageIO
{
public:
    static bool readImageColorSpace(const std::string& filename, std::string& outColorSpace);
    static bool readImage(CmImage& target, const std::string& filename, const std::string& colorSpace, std::string& outError);

    // Save as OpenEXR in RGB32F
    static bool writeImage(CmImage& source, const std::string& filename, const std::string& colorSpace, std::string& outError);
};