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

#include "Utils/OpenGL/GlContext.h"
#include "Utils/OpenGL/GlFullPlaneVertices.h"

#include "Utils/FileDialogs.h"
#include "Utils/ImageTransform.h"
#include "Utils/NumberHelpers.h"
#include "Utils/Misc.h"

struct ImageSlot
{
    std::string id;
    std::string name;
    bool canLoad;
    std::shared_ptr<CmImage> viewImage;

    CmImage* internalImage;
    bool* updateIndicator;

    void indicateUpdate();
};

constexpr const char* DIALOG_TITLE_MOVETO = "Move To";

struct DialogParams_MoveTo
{
    int selSourceSlot = -1;
    int selDestSlot = -1;
    bool preserveOriginal = false;
};

void layoutAll();

void layoutImagePanels();
void layoutColorManagement();
void layoutMisc();
void layoutConvolution();
void layoutDiffraction();
void layoutDebug();

bool layoutImageTransformParams(const std::string& imageName, const std::string& imGuiID, ImageTransformParams& params);

void imGuiDiv();
void imGuiHorzDiv();
void imGuiBold(const std::string& s);
void imGuiText(const std::string& s, bool isError, bool newLine);
bool imGuiSliderUInt(const std::string& label, uint32_t* v, uint32_t min, uint32_t max);
bool imGuiInputUInt(const std::string& label, uint32_t* v);
bool imGuiCombo(const std::string& label, const std::vector<std::string>& items, int* selectedIndex, bool fullWidth);
void imGuiDialogs();

void addImageSlot(const std::string& id, const std::string& name, bool canLoad, CmImage* internalImage = nullptr, bool* updateIndicator = nullptr);
ImageSlot& getSlotByID(const std::string& id);

void loadImageToSlot(ImageSlot& slot, const std::string& filename);
bool browseImageForSlot(ImageSlot& slot);
bool saveImageFromSlot(ImageSlot& slot);

inline ImVec2 btnSize();
inline ImVec2 dlgBtnSize();

bool setupGLFW();
bool setupImGui();
void applyStyle_RealBloomGray();
void cleanUp();

void dragDropCallback(GLFWwindow* window, int count, const char** paths);
