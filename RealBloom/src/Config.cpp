#include "Config.h"

using namespace std;

std::string Config::CFG_FILENAME = getLocalPath("config.ini");

// Const
const char* Config::S_APP_TITLE = "RealBloom";
const char* Config::S_APP_VERSION = "0.5.0-dev";
const char* Config::S_APP_LOCALE = "en_US.UTF-8";

const uint32_t Config::S_WINDOW_WIDTH = 1440;
const uint32_t Config::S_WINDOW_HEIGHT = 810;
const float Config::S_UI_MAX_SCALE = 1.50f;
const float Config::S_UI_MIN_SCALE = 0.75f;

const char* Config::S_GITHUB_URL = "https://github.com/bean-mhm/realbloom";
const char* Config::S_DOCS_URL = "https://github.com/bean-mhm/realbloom/blob/main/USAGE.md";

// Variable
float Config::UI_SCALE = 1.0f;

void Config::load()
{
    INIReader reader(CFG_FILENAME);

    if (reader.ParseError() != 0)
    {
        printWarning(__FUNCTION__, "",
            strFormat("Failed to read config from \"%s\".", CFG_FILENAME.c_str()));
    }
    else
    {
        UI_SCALE = reader.GetFloat("Interface", "Scale", 1.0f);
    }
}

void Config::save()
{
    ofstream outFile;
    outFile.open(CFG_FILENAME, ofstream::out | ofstream::trunc);
    if (!outFile)
    {
        printWarning(__FUNCTION__, "",
            strFormat("Failed to write config to \"%s\".", CFG_FILENAME.c_str()));
    }
    else
    {
        string s = strFormat(
            "[Interface]\nScale=%f\n",
            UI_SCALE);

        outFile.write(s.c_str(), s.size());
        outFile.flush();
        outFile.close();
    }
}
