#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <stdint.h>

#include <inih/INIReader.h>
#include "Utils/Misc.h"

class Config
{
public:
    static std::string CFG_FILENAME;

    // const
    static const char* S_APP_TITLE;
    static const char* S_APP_VERSION;
    static const char* S_APP_LOCALE;
    static const uint32_t S_WINDOW_WIDTH;
    static const uint32_t S_WINDOW_HEIGHT;
    static const float S_UI_MAX_SCALE;
    static const float S_UI_MIN_SCALE;
    static const char* S_GITHUB_URL;
    static const char* S_DOCS_URL;

    // variable
    static float UI_SCALE;

    static void load();
    static void save();
};