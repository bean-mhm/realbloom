#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstdint>

#include <pugixml/pugixml.hpp>

#include "Utils/Misc.h"

// Program Details and Settings (Global)
class Config
{
public:
    Config() = delete;
    Config(const Config&) = delete;
    Config& operator= (const Config&) = delete;

    // Const
    static const char* APP_TITLE;
    static const char* APP_VERSION;
    static const char* APP_LOCALE;
    static const uint32_t WINDOW_WIDTH;
    static const uint32_t WINDOW_HEIGHT;
    static const float UI_MAX_SCALE;
    static const float UI_MIN_SCALE;
    static const char* GITHUB_URL;
    static const char* DOCS_URL;

    // Variable
    static float UI_SCALE;

    static void load();
    static void save();

private:
    static std::string CFG_FILENAME;

};
