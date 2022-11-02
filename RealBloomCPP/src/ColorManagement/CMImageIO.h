#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <memory>

#include "OpenImageIO/imageio.h"

#include "CMS.h"
#include "CMImage.h"

#include "../Utils/Misc.h"

class CMImageIO
{
public:
    static bool readImage(CMImage& target, const std::string& filename, const std::string& colorSpace, std::string& outError);
    static bool writeImage(CMImage& source, const std::string& filename, const std::string& colorSpace, std::string& outError);
};