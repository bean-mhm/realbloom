#pragma once

#define NOMINMAX
#include <Windows.h>

#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <mutex>
#include <memory>
#include <future>
#include <functional>
#include <algorithm>
#include <stdint.h>
#include <locale>
#include <clocale>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "ColorManagement/CMS.h"
#include "ColorManagement/CMF.h"
#include "ColorManagement/CmXYZ.h"
#include "ColorManagement/CmImage.h"
#include "ColorManagement/CmImageIO.h"

#include "RealBloom/DiffractionPattern.h"
#include "RealBloom/Dispersion.h"
#include "RealBloom/Convolution.h"

#include "Async.h"
#include "Config.h"
#include "CLI.h"

#include "Utils/FileDialogs.h"
#include "Utils/NumberHelpers.h"
#include "Utils/Misc.h"

void layout();

void imGuiDiv();
void imGuiBold(const std::string& s);
void imGuiText(const std::string& s, bool isError, bool newLine);

bool lb1ItemGetter(void* data, int index, const char** outText);
bool comboItemGetter(void* data, int index, const char** outText);
bool imGuiCombo(const std::string& label, const std::vector<std::string>& items, int* selectedIndex, bool fullWidth);

void addImage(const std::string& id, const std::string& name);
CmImage* getImageByID(const std::string& id);

void loadImage(CmImage& image, const std::string& dlgID, std::function<void()> onLoad, std::string& outError);
void saveImage(CmImage& image, const std::string& dlgID, std::string& outError);

void imGuiDialogs();
void updateDiffParams();
void updateDispParams();
void updateConvParams();

inline ImVec2 btnSize();
inline ImVec2 dlgBtnSize();

bool setupGLFW();
bool setupImGui();
void applyStyle_RealBloomGray();
void cleanUp();

struct UiVars
{
    // Diffraction Pattern
    bool dp_grayscale = false;

    // Dispersion
    float ds_exposure = 0.0f;
    float ds_contrast = 0.0f;
    float ds_color[3]{ 1.0f, 1.0f, 1.0f };
    float ds_amount = 0.4f;
    int ds_steps = 32;
    bool dispParamsChanged = false;

    // Dispersion Method
    int ds_method = 0;
    int ds_numThreads = getDefNumThreads();

    // Convolution Kernel
    float cv_kernelExposure = 0.0f;
    float cv_kernelContrast = 0.0f;
    float cv_kernelColor[3]{ 1.0f, 1.0f, 1.0f };
    float cv_kernelRotation = 0.0f;
    bool cv_kernelLockScale = true;
    float cv_kernelScale[2] = { 1.0f, 1.0f };
    bool cv_kernelLockCrop = true;
    float cv_kernelCrop[2] = { 1.0f, 1.0f };
    bool cv_kernelPreviewCenter = false;
    float cv_kernelCenter[2] = { 0.5f, 0.5f };
    bool convParamsChanged = false;

    // Convolution Method
    int cv_method = 0;
    int cv_numThreads = getDefNumThreads();
    int cv_numChunks = 10;
    int cv_chunkSleep = 0;

    // Convolution Threshold
    float cv_convThreshold = 0.0f;
    float cv_convKnee = 0.0f;
    bool convThresholdChanged = false;
    bool convThresholdSwitchImage = false;

    // Kernel Auto-Exposure
    bool cv_kernelAutoExposure = true;

    // Convolution Mix (Layers)
    bool cm_additive = false;
    float cm_inputMix = 1.0f;       // for additive blending
    float cm_convMix = 0.2f;        // for additive blending
    float cm_mix = 0.2f;            // for normal blending
    float cm_convExposure = 0.0f;
    bool convMixParamsChanged = false;

    // Color Management
    float cms_exposure = 0.0f;
    bool cmsParamsChanged = false;
};
