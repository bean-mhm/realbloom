#pragma once

#include <iostream>
#include <string>
#include <filesystem>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

constexpr const char* OCIO_CONFIG_PATH = "./ocio/config.ocio";

class CMS
{
private:
    struct CMVars
    {
        OCIO::ConstConfigRcPtr config = nullptr;

        std::vector<std::string> colorSpaces;
        std::vector<std::string> displays;
        std::vector<std::string> views;
        std::vector<std::string> looks;

        std::string activeDisplay = "";
        std::string activeView = "";
        std::string activeLook = "";

        void retrieveColorSpaces();
        void retrieveDisplays();
        void retrieveViews();
        void retrieveLooks();
    };

    static CMVars S_VARS;

public:
    static bool init();
    static OCIO::ConstConfigRcPtr getConfig();

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
};