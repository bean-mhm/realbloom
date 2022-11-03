#include "CMS.h"

CMS::CMVars CMS::S_VARS;

void CMS::CMVars::retrieveColorSpaces()
{
    colorSpaces.clear();
    try
    {
        size_t numColorSpaces = config->getNumColorSpaces();
        for (size_t i = 0; i < numColorSpaces; i++)
            colorSpaces.push_back(config->getColorSpaceNameByIndex(i));
    } catch (OCIO::Exception& exception)
    {
        std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
    }
}

void CMS::CMVars::retrieveDisplays()
{
    displays.clear();
    try
    {
        size_t numDisplays = config->getNumDisplays();
        for (size_t i = 0; i < numDisplays; i++)
            displays.push_back(config->getDisplay(i));
    } catch (OCIO::Exception& exception)
    {
        std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
    }
}

void CMS::CMVars::retrieveViews()
{
    views.clear();
    try
    {
        size_t numViews = config->getNumViews(activeDisplay.c_str());
        for (size_t i = 0; i < numViews; i++)
            views.push_back(config->getView(activeDisplay.c_str(), i));
    } catch (OCIO::Exception& exception)
    {
        std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
    }
}

void CMS::CMVars::retrieveLooks()
{
    looks.clear();
    looks.push_back("None");
    try
    {
        size_t numLooks = config->getNumLooks();
        for (size_t i = 0; i < numLooks; i++)
            looks.push_back(config->getLookNameByIndex(i));
    } catch (OCIO::Exception& exception)
    {
        std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
    }
}

bool CMS::init()
{
    bool success = false;
    try
    {
        S_VARS.config = OCIO::Config::CreateFromFile(OCIO_CONFIG_PATH);

        S_VARS.activeDisplay = S_VARS.config->getDefaultDisplay();
        S_VARS.activeView = S_VARS.config->getDefaultView(S_VARS.activeDisplay.c_str());

        S_VARS.retrieveColorSpaces();
        S_VARS.retrieveDisplays();
        S_VARS.retrieveViews();
        S_VARS.retrieveLooks();

        success = true;
    } catch (OCIO::Exception& exception)
    {
        std::cerr << "OpenColorIO Error: " << exception.what() << std::endl;
    }
    return success;
}

OCIO::ConstConfigRcPtr CMS::getConfig()
{
    return S_VARS.config;
}

const std::vector<std::string>& CMS::getAvailableColorSpaces()
{
    return S_VARS.colorSpaces;
}

const std::vector<std::string>& CMS::getAvailableDisplays()
{
    return S_VARS.displays;
}

const std::vector<std::string>& CMS::getAvailableViews()
{
    return S_VARS.views;
}

const std::vector<std::string>& CMS::getAvailableLooks()
{
    return S_VARS.looks;
}

const std::string& CMS::getActiveDisplay()
{
    return S_VARS.activeDisplay;
}

const std::string& CMS::getActiveView()
{
    return S_VARS.activeView;
}

const std::string& CMS::getActiveLook()
{
    return S_VARS.activeLook;
}

void CMS::setActiveDisplay(const std::string& newDisplay)
{
    S_VARS.activeDisplay = newDisplay;
    S_VARS.activeView = S_VARS.config->getDefaultView(S_VARS.activeDisplay.c_str());
    S_VARS.retrieveViews();
}

void CMS::setActiveView(const std::string& newView)
{
    S_VARS.activeView = newView;
}

void CMS::setActiveLook(const std::string& newLook)
{
    S_VARS.activeLook = newLook;
}
