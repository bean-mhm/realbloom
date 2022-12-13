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
#include "CmImage.h"

#include "../Utils/Misc.h"

class CmImageIO
{
public:
    CmImageIO() = delete;
    CmImageIO(const CmImageIO&) = delete;
    CmImageIO& operator= (const CmImageIO&) = delete;

    static bool readImageColorSpace(const std::string& filename, std::string& outColorSpace);
    static void readImage(CmImage& target, const std::string& filename, const std::string& colorSpace);

    // Save as OpenEXR in RGB32F
    static void writeImage(CmImage& source, const std::string& filename, const std::string& colorSpace);
};