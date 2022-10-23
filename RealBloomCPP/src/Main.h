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

void layout();

void imGuiText(const std::string& text, bool isError, bool newLine);
bool lb1ItemGetter(void* data, int index, const char** outText);
bool cb1ItemGetter(void* data, int index, const char** outText);

Image32Bit* getImage(const std::string& id);
bool loadImage(std::string filename, ImageFormat format, Image32Bit& image, LoadImageResult& outResult);
bool saveDialog(std::string extension, std::string& outFilename);

void renderDiffPattern();
void updateConvParams();

bool setupGLFW();
bool setupImGui();
void applyStyle_RealBloom();
void applyStyle_Blender();
void cleanUp();

struct UiVars
{
    bool dp_grayscale = false;

    float dp_contrast = 0.0f;
    float dp_multiplier = 2.0f;
    bool dpParamsChanged = false;

    float ds_dispersionAmount = 0.4f;
    int ds_dispersionSteps = 32;
    float ds_dispersionCol[3]{ 1.0f, 1.0f, 1.0f };

    float cv_kernelRotation = 0.0f;
    float cv_kernelScale[2] = { 1.0f, 1.0f };
    float cv_kernelCrop[2] = { 1.0f, 1.0f };
    float cv_kernelCenter[2] = { 0.5f, 0.5f };
    bool cv_kernelPreviewCenter = false;
    float cv_kernelContrast = 0.0f;
    float cv_kernelIntensity = 1.0f;
    bool convParamsChanged = false;

    float cv_convThreshold = 0.5f;
    float cv_convKnee = 0.0f;
    bool convThresholdChanged = false;
    bool convThresholdSwitchImage = false;

    int cv_deviceType = 0;
    int cv_maxThreads = std::max((uint32_t)1, std::thread::hardware_concurrency());
    int cv_halfMaxThreads = std::max(1, cv_maxThreads / 2);
    int cv_numThreads = cv_halfMaxThreads;
    int cv_numChunks = 10;
    int cv_chunkSleep = 0;

    bool cm_additive = true;
    float cm_inputMix = 1.0f;       // for additive blending
    float cm_convMix = 1.0f;        // for additive blending
    float cm_mix = 0.2f;            // for normal blending
    float cm_convIntensity = 1.0f;  // for normal blending
    bool convMixParamsChanged = false;
};