#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstdint>

#include <pugixml/pugixml.hpp>

#include "Utils/Misc.h"

class Config
{
private:
    static std::string CFG_FILENAME;

public:
    // Const
    static const char* S_APP_TITLE;
    static const char* S_APP_VERSION;
    static const char* S_APP_LOCALE;
    static const uint32_t S_WINDOW_WIDTH;
    static const uint32_t S_WINDOW_HEIGHT;
    static const float S_UI_MAX_SCALE;
    static const float S_UI_MIN_SCALE;
    static const char* S_GITHUB_URL;
    static const char* S_DOCS_URL;

    // Variable
    static float UI_SCALE;

    static void load();
    static void save();

};
