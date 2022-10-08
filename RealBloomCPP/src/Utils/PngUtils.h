#pragma once

#include <stdint.h>
#include <vector>
#include <filesystem>
#include <string>
#include <lodepng/lodepng.h>
#include "NumberHelpers.h"

bool loadPNG(std::string filename, std::vector<float>& outBuffer, uint32_t& outWidth, uint32_t& outHeight, std::string& outError);
bool savePNG(std::string filename, float* buffer, bool isRGBA, bool FlipY, uint32_t width, uint32_t height, std::string& outError);