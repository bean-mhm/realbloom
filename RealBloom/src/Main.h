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
#include <locale>
#include <clocale>
#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <nfde/nfd.h>

#include "Async.h"
#include "Config.h"
#include "CLI.h"

#include "ColorManagement/CMS.h"
#include "ColorManagement/CMF.h"
#include "ColorManagement/CmXYZ.h"
#include "ColorManagement/CmImage.h"
#include "ColorManagement/CmImageIO.h"

#include "RealBloom/Diffraction.h"
#include "RealBloom/Dispersion.h"
#include "RealBloom/Convolution.h"

#include "Utils/FileDialogs.h"
#include "Utils/ImageTransform.h"
#include "Utils/OpenGL/GlContext.h"
#include "Utils/NumberHelpers.h"
#include "Utils/Misc.h"

void layoutAll();

void layoutImagePanels();
void layoutColorManagement();
void layoutMisc();
void layoutConvolution();
void layoutDiffraction();

bool layoutImageTransformParams(const std::string& imageName, const std::string& imGuiID, ImageTransformParams& params);

void imGuiDiv();
void imGuiBold(const std::string& s);
void imGuiText(const std::string& s, bool isError, bool newLine);
bool imGuiSliderUInt(const std::string& label, uint32_t* v, uint32_t min, uint32_t max);
bool imGuiInputUInt(const std::string& label, uint32_t* v);
bool imGuiCombo(const std::string& label, const std::vector<std::string>& items, int* selectedIndex, bool fullWidth);
void imGuiDialogs();

void addImage(const std::string& id, const std::string& name);
CmImage* getImageByID(const std::string& id);

bool openImage(CmImage& image, const std::string& dlgID, std::string& outError);
bool saveImage(CmImage& image, const std::string& dlgID, std::string& outError);

inline ImVec2 btnSize();
inline ImVec2 dlgBtnSize();

bool setupGLFW();
bool setupImGui();
void applyStyle_RealBloomGray();
void cleanUp();

struct UiVars
{
    // Color Management (CMS)
    float cms_exposure = 0.0f;
    bool cmsParamsChanged = false;
};
