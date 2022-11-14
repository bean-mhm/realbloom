#pragma once

#include <iostream>
#include <string>
#include <filesystem>
#include <memory>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

#include "OcioShader.h"
#include "../Utils/GlUtils.h"
#include "../Utils/Misc.h"

constexpr const char* CMS_INTERNAL_CONFIG_PATH = "./assets/internal/ocio/config.ocio";
constexpr const char* CMS_INTERNAL_XYZ_IE = "Linear CIE-XYZ I-E";
constexpr const char* CMS_CONFIG_PATH = "./ocio/config.ocio";
constexpr const bool CMS_USE_GPU = true;

class CMS
{
private:
    struct CmVars
    {
        OCIO::ConstConfigRcPtr config;
        OCIO::ConstConfigRcPtr internalConfig;

        std::string internalXyzSpace = "";
        std::string workingSpace = "";
        std::string workingSpaceDesc = "";

        std::vector<std::string> internalColorSpaces;
        std::vector<std::string> colorSpaces;
        std::vector<std::string> displays;
        std::vector<std::string> views;
        std::vector<std::string> looks;

        std::string activeDisplay = "";
        std::string activeView = "";
        std::string activeLook = "";

        float exposure = 0.0f;

        OCIO::GroupTransformRcPtr groupTransform;
        OCIO::ConstProcessorRcPtr processor;
        OCIO::ConstCPUProcessorRcPtr cpuProcessor;
        OCIO::ConstGPUProcessorRcPtr gpuProcessor;

        std::shared_ptr<OcioShader> shader;

        void retrieveColorSpaces();
        void retrieveDisplays();
        void retrieveViews();
        void retrieveLooks();
    };
    static CmVars* S_VARS;
    static SimpleState S_STATE;

    static void updateProcessors();

public:
    CMS() = delete;
    CMS(const CMS&) = delete;
    CMS& operator= (const CMS&) = delete;

    static bool init();
    static void cleanUp();

    static OCIO::ConstConfigRcPtr getInternalConfig();
    static OCIO::ConstConfigRcPtr getConfig();

    static const std::string& getInternalXyzSpace();
    static const std::string& getWorkingSpace();
    static const std::string& getWorkingSpaceDesc();

    static const std::vector<std::string>& getInternalColorSpaces();
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

    static bool ok();
    static std::string getError();

    static OCIO::ConstCPUProcessorRcPtr getCpuProcessor();
    static OCIO::ConstGPUProcessorRcPtr getGpuProcessor();
    static std::shared_ptr<OcioShader> getShader();
};

std::string getColorSpaceDesc(OCIO::ConstConfigRcPtr config, const std::string& colorSpace);
