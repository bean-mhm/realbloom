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

        OCIO::ConstColorSpaceRcPtr sceneLinear = S_VARS.config->getColorSpace(OCIO::ROLE_SCENE_LINEAR);
        S_VARS.workingSpace = sceneLinear->getName();
        S_VARS.workingSpaceDesc = sceneLinear->getDescription();

        S_VARS.activeDisplay = S_VARS.config->getDefaultDisplay();
        S_VARS.activeView = S_VARS.config->getDefaultView(S_VARS.activeDisplay.c_str());

        S_VARS.retrieveColorSpaces();
        S_VARS.retrieveDisplays();
        S_VARS.retrieveViews();
        S_VARS.retrieveLooks();

        updateProcessors();

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

const std::string& CMS::getWorkingSpace()
{
    return S_VARS.workingSpace;
}

const std::string& CMS::getWorkingSpaceDesc()
{
    return S_VARS.workingSpaceDesc;
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

    updateProcessors();
}

void CMS::setActiveView(const std::string& newView)
{
    S_VARS.activeView = newView;
    updateProcessors();
}

void CMS::setActiveLook(const std::string& newLook)
{
    S_VARS.activeLook = newLook;
    updateProcessors();
}

float CMS::getExposure()
{
    return S_VARS.exposure;
}

void CMS::setExposure(float newExposure)
{
    S_VARS.exposure = newExposure;
}

void CMS::updateProcessors()
{
    S_VARS.hasProcessors = false;
    try
    {
        // Create group transform
        S_VARS.groupTransform = OCIO::GroupTransform::Create();

        // Add display view transform
        OCIO::DisplayViewTransformRcPtr displayViewTransform = OCIO::DisplayViewTransform::Create();
        displayViewTransform->setSrc(OCIO::ROLE_SCENE_LINEAR);
        displayViewTransform->setDisplay(S_VARS.activeDisplay.c_str());
        displayViewTransform->setView(S_VARS.activeView.c_str());
        S_VARS.groupTransform->appendTransform(displayViewTransform);

        // Add look transform
        std::string lookName = S_VARS.activeLook;
        if ((!lookName.empty()) && (lookName != "None"))
        {
            OCIO::ConstLookRcPtr look = S_VARS.config->getLook(lookName.c_str());
            S_VARS.groupTransform->appendTransform(look->getTransform()->createEditableCopy());
        }

        // Get processor
        S_VARS.processor = S_VARS.config->getProcessor(S_VARS.groupTransform);
        S_VARS.cpuProcessor = S_VARS.processor->getDefaultCPUProcessor();

        // Won't be called if an error occurs
        S_VARS.hasProcessors = true;
    } catch (OCIO::Exception& exception)
    {
        std::cerr << "OpenColorIO Error [CMS::updateProcessor()]: " << exception.what() << std::endl;
    }
}

bool CMS::hasProcessors()
{
    return S_VARS.hasProcessors;
}

OCIO::ConstCPUProcessorRcPtr CMS::getCpuProcessor()
{
    if (S_VARS.hasProcessors)
        return S_VARS.cpuProcessor;
    return nullptr;
}
