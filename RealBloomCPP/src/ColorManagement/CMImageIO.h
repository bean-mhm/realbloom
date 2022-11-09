#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <memory>

#include "OpenImageIO/imageio.h"

#include "CMS.h"
#include "CMImage.h"

#include "../Utils/Misc.h"

class CmImageIO
{
public:
    static bool readImage(CmImage& target, const std::string& filename, const std::string& colorSpace, std::string& outError);

    // Save as OpenEXR in the RGB32F format
    static bool writeImage(CmImage& source, const std::string& filename, const std::string& colorSpace, std::string& outError);
};