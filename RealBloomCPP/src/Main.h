#pragma once

#define NOMINMAX
#include <Windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <mutex>
#include <memory>
#include <stdint.h>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "Async.h"

#include "Utils/Image32Bit.h"
#include "Utils/PngUtils.h"
#include "Utils/TiffUtils.h"
#include "nfd/nfd.h"
#include "Config.h"

#include "RealBloom/DiffractionPattern.h"
#include "RealBloom/Dispersion.h"
#include "RealBloom/Convolution.h"

#define FILTER_LIST_TIFF "tif,tiff"
#define FILTER_LIST_PNG  "png"

enum class ImageFormat
{
    PNG, TIFF
};

struct LoadImageResult
{
    bool success = true;
    std::string error = "";
};

bool setupGLFW();
bool setupImGui();
void applyStyle();
void cleanUp();

void layout();
Image32Bit* getImage(uint32_t id);
bool loadImage(std::string filename, ImageFormat format, Image32Bit& image, LoadImageResult& outResult);
bool saveDialog(std::string extension, std::string& outFilename);
void renderDiffPattern();
void updateConvParams();