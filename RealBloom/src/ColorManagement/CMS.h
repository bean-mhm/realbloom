#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <memory>

#include "ocio/OpenColorIO.h"
namespace OCIO = OpenColorIO_v2_1;

#include "OcioShader.h"
#include "../Utils/OpenGL/GlUtils.h"
#include "../Utils/Status.h"
#include "../Utils/Misc.h"
#include "../CLI.h"

class CMS
{
private:
    static std::string CONFIG_PATH;
    static std::string INTERNAL_CONFIG_PATH;
    static std::string INTERNAL_XYZ_IE;
    static bool USE_GPU;

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

        bool initProcessors = false;
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
    static BaseStatus S_STATUS;

    static void ensureProcessors();

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
    static const std::vector<std::string>& getColorSpaces();
    static const std::vector<std::string>& getDisplays();
    static const std::vector<std::string>& getViews();
    static const std::vector<std::string>& getLooks();

    static const std::string& getActiveDisplay();
    static const std::string& getActiveView();
    static const std::string& getActiveLook();

    static void setActiveDisplay(const std::string& display);
    static void setActiveView(const std::string& view);
    static void setActiveLook(const std::string& look);

    static float getExposure();
    static void setExposure(float exposure);

    static void updateProcessors();
    static OCIO::ConstCPUProcessorRcPtr getCpuProcessor();
    static OCIO::ConstGPUProcessorRcPtr getGpuProcessor();
    static std::shared_ptr<OcioShader> getShader();

    static const BaseStatus& getStatus();
    static void ensureOK();

    static bool usingGPU();

    static std::string resolveColorSpace(const std::string& name, bool preserve = true);
    static std::string getColorSpaceDesc(OCIO::ConstConfigRcPtr config, const std::string& colorSpace);
    static std::array<float, 4> getDisplayColor(std::array<float, 4> v);
    static std::array<float, 3> getDisplayColor(const std::array<float, 3>& v);
};
