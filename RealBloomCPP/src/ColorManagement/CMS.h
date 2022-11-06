#pragma once

#include <iostream>
#include <string>
#include <filesystem>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

#include "../Utils/Misc.h"

constexpr const char* OCIO_CONFIG_PATH = "./ocio/config.ocio";
constexpr const bool CMS_USE_GPU = false;

class CMS
{
private:
    struct CMVars
    {
        OCIO::ConstConfigRcPtr config = nullptr;

        std::string workingSpace = "";
        std::string workingSpaceDesc = "";

        std::vector<std::string> colorSpaces;
        std::vector<std::string> displays;
        std::vector<std::string> views;
        std::vector<std::string> looks;

        std::string activeDisplay = "";
        std::string activeView = "";
        std::string activeLook = "";

        float exposure = 0.0f;

        bool hasProcessors = false;
        OCIO::GroupTransformRcPtr groupTransform;
        OCIO::ConstProcessorRcPtr processor;
        OCIO::ConstCPUProcessorRcPtr cpuProcessor;
        OCIO::ConstGPUProcessorRcPtr gpuProcessor;

        void retrieveColorSpaces();
        void retrieveDisplays();
        void retrieveViews();
        void retrieveLooks();
    };
    static CMVars S_VARS;

    static void updateProcessors();

public:
    static bool init();
    static OCIO::ConstConfigRcPtr getConfig();

    static const std::string& getWorkingSpace();
    static const std::string& getWorkingSpaceDesc();

    static const std::vector<std::string>& getAvailableColorSpaces();
    static const std::vector<std::string>& getAvailableDisplays();
    static const std::vector<std::string>& getAvailableViews();
    static const std::vector<std::string>& getAvailableLooks();

    static const std::string& getActiveDisplay();
    static const std::string& getActiveView();
    static const std::string& getActiveLook();

    static void setActiveDisplay(const std::string& newDisplay);
    static void setActiveView(const std::string& newView);
    static void setActiveLook(const std::string& newLook);

    static float getExposure();
    static void setExposure(float newExposure);

    static bool hasProcessors();
    static OCIO::ConstCPUProcessorRcPtr getCpuProcessor();
    static OCIO::ConstGPUProcessorRcPtr getGpuProcessor();
};