#include "Config.h"

using namespace std;

constexpr const char* cfgFilename = "config.ini";

// const
const char* Config::S_APP_TITLE = "RealBloom";
const char* Config::S_APP_VERSION = "0.2.0-beta";
const uint32_t Config::S_WINDOW_WIDTH = 1366;
const uint32_t Config::S_WINDOW_HEIGHT = 768;
const float Config::S_UI_MAX_SCALE = 1.50f;
const float Config::S_UI_MIN_SCALE = 0.75f;

// variable
float Config::UI_SCALE = 1.0f;

void Config::load()
{
    INIReader reader(cfgFilename);

    if (reader.ParseError() != 0)
    {
        std::cout << "Failed to read config from \"" << cfgFilename << "\".\n";
    } else
    {
        UI_SCALE = reader.GetFloat("Interface", "Scale", 1.0f);
    }
}

void Config::save()
{
    ofstream outFile;
    outFile.open(cfgFilename, ofstream::out | ofstream::trunc);
    if (!outFile)
    {
        std::cout << "Failed to write config to \"" << cfgFilename << "\".\n";
    } else
    {
        string s = stringFormat(
            "[Interface]\nScale=%f\n",
            UI_SCALE);

        outFile.write(s.c_str(), s.size());
        outFile.flush();
        outFile.close();
    }
}