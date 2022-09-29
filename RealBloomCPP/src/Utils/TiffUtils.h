#pragma once

#include <stdint.h>
#include <vector>
#include <filesystem>
#include <string>

#include <libtiff/tiffio.h>

bool loadTIFF(std::string filename, std::vector<float>& outBuffer, uint32_t& outWidth, uint32_t& outHeight, std::string& outError);
bool saveTIFF(std::string filename, float* buffer, uint32_t width, uint32_t height, std::string& outError);