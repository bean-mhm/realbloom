#include "Config.h"

using namespace std;

std::string Config::CFG_FILENAME = getLocalPath("config.xml");

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
    std::string stage = "";

    try
    {
        if (!std::filesystem::exists(CFG_FILENAME))
            throw std::exception(strFormat("Config file \"%s\" doesn't exist.", CFG_FILENAME.c_str()).c_str());

        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(CFG_FILENAME.c_str());

        if (!result)
            throw std::exception(strFormat(
                "Failed to read the config from \"%s\": %s (Offset: %d)",
                CFG_FILENAME.c_str(),
                result.description(),
                result.offset
            ).c_str());

        // Root node
        pugi::xml_node root = doc.child(S_APP_TITLE).child("Config");

        // Interface
        {
            pugi::xml_node interfaceNode = root.child("Interface");

            // Scale
            stage = "Interface/Scale";
            std::string scaleValue = interfaceNode.child("Scale").text().as_string();
            if (!scaleValue.empty())
            {
                UI_SCALE = std::stof(scaleValue);
                UI_SCALE = fminf(fmaxf(Config::UI_SCALE, Config::S_UI_MIN_SCALE), Config::S_UI_MAX_SCALE);
            }
        }
    }
    catch (const std::exception& e)
    {
        printWarning(__FUNCTION__, stage, e.what());
    }
}

void Config::save()
{
    std::string stage = "";

    try
    {
        ofstream outFile;
        outFile.open(CFG_FILENAME, ofstream::out | ofstream::trunc);

        if (!outFile)
            throw std::exception(strFormat("Failed to open config file \"%s\".", CFG_FILENAME.c_str()).c_str());

        pugi::xml_document doc;

        // Root node
        pugi::xml_node root = doc.append_child(S_APP_TITLE).append_child("Config");

        // Interface
        {
            pugi::xml_node interfaceNode = root.append_child("Interface");

            // Scale
            pugi::xml_node scaleNode = interfaceNode.append_child("Scale");
            scaleNode.append_child(pugi::node_pcdata).set_value(strFormat("%f", UI_SCALE).c_str());
        }

        // Write to the file
        stage = "Write";
        doc.save(outFile, "  ");
        outFile.flush();
        outFile.close();
    }
    catch (const std::exception& e)
    {
        printWarning(__FUNCTION__, stage, e.what());
    }
}
