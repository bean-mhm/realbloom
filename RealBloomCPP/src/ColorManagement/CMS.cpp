#include "CMS.h"

std::string CMS::CONFIG_PATH = getLocalPath("ocio/config.ocio");
std::string CMS::INTERNAL_CONFIG_PATH = getLocalPath("assets/internal/ocio/config.ocio");
std::string CMS::INTERNAL_XYZ_IE = "Linear CIE-XYZ I-E";
bool CMS::USE_GPU = true;

CMS::CmVars* CMS::S_VARS = nullptr;
SimpleState CMS::S_STATE;

void CMS::CmVars::retrieveColorSpaces()
{
    internalColorSpaces.clear();
    try
    {
        size_t num = internalConfig->getNumColorSpaces();
        for (size_t i = 0; i < num; i++)
            internalColorSpaces.push_back(internalConfig->getColorSpaceNameByIndex(i));
    } catch (OCIO::Exception& e)
    {
        printErr(__FUNCTION__, "Internal config", e.what());
    }

    colorSpaces.clear();
    try
    {
        size_t num = config->getNumColorSpaces();
        for (size_t i = 0; i < num; i++)
            colorSpaces.push_back(config->getColorSpaceNameByIndex(i));
    } catch (OCIO::Exception& e)
    {
        printErr(__FUNCTION__, "User config", e.what());
    }
}

void CMS::CmVars::retrieveDisplays()
{
    displays.clear();
    try
    {
        size_t num = config->getNumDisplays();
        for (size_t i = 0; i < num; i++)
            displays.push_back(config->getDisplay(i));
    } catch (OCIO::Exception& e)
    {
        printErr(__FUNCTION__, e.what());
    }
}

void CMS::CmVars::retrieveViews()
{
    views.clear();
    try
    {
        size_t num = config->getNumViews(activeDisplay.c_str());
        for (size_t i = 0; i < num; i++)
            views.push_back(config->getView(activeDisplay.c_str(), i));
    } catch (OCIO::Exception& e)
    {
        printErr(__FUNCTION__, e.what());
    }
}

void CMS::CmVars::retrieveLooks()
{
    looks.clear();
    looks.push_back("None");
    try
    {
        size_t num = config->getNumLooks();
        for (size_t i = 0; i < num; i++)
            looks.push_back(config->getLookNameByIndex(i));
    } catch (OCIO::Exception& e)
    {
        printErr(__FUNCTION__, e.what());
    }
}

bool CMS::init()
{
    S_VARS = new CmVars();
    std::string stage;
    try
    {
        // Internal config
        stage = "Internal config";
        {
            S_VARS->internalConfig = OCIO::Config::CreateFromFile(INTERNAL_CONFIG_PATH.c_str());
            S_VARS->internalXyzSpace = INTERNAL_XYZ_IE;
        }

        // User config
        stage = "User config";
        {
            S_VARS->config = OCIO::Config::CreateFromFile(CONFIG_PATH.c_str());

            OCIO::ConstColorSpaceRcPtr sceneLinear = S_VARS->config->getColorSpace(OCIO::ROLE_SCENE_LINEAR);
            S_VARS->workingSpace = sceneLinear->getName();
            S_VARS->workingSpaceDesc = sceneLinear->getDescription();

            S_VARS->activeDisplay = S_VARS->config->getDefaultDisplay();
            S_VARS->activeView = S_VARS->config->getDefaultView(S_VARS->activeDisplay.c_str());
        }

        S_VARS->retrieveColorSpaces();
        S_VARS->retrieveDisplays();
        S_VARS->retrieveViews();
        S_VARS->retrieveLooks();

        S_STATE.setOk();
    } catch (std::exception& e)
    {
        S_STATE.setError(printErr(__FUNCTION__, stage, e.what()));
    }

    if (ok())
        updateProcessors();

    return S_STATE.ok();
}

void CMS::cleanUp()
{
    DELPTR(S_VARS);
}

OCIO::ConstConfigRcPtr CMS::getInternalConfig()
{
    return S_VARS->internalConfig;
}

OCIO::ConstConfigRcPtr CMS::getConfig()
{
    return S_VARS->config;
}

const std::string& CMS::getInternalXyzSpace()
{
    return S_VARS->internalXyzSpace;
}

const std::string& CMS::getWorkingSpace()
{
    return S_VARS->workingSpace;
}

const std::string& CMS::getWorkingSpaceDesc()
{
    return S_VARS->workingSpaceDesc;
}

const std::vector<std::string>& CMS::getInternalColorSpaces()
{
    return S_VARS->internalColorSpaces;
}

const std::vector<std::string>& CMS::getAvailableColorSpaces()
{
    return S_VARS->colorSpaces;
}

const std::vector<std::string>& CMS::getAvailableDisplays()
{
    return S_VARS->displays;
}

const std::vector<std::string>& CMS::getAvailableViews()
{
    return S_VARS->views;
}

const std::vector<std::string>& CMS::getAvailableLooks()
{
    return S_VARS->looks;
}

const std::string& CMS::getActiveDisplay()
{
    return S_VARS->activeDisplay;
}

const std::string& CMS::getActiveView()
{
    return S_VARS->activeView;
}

const std::string& CMS::getActiveLook()
{
    return S_VARS->activeLook;
}

void CMS::setActiveDisplay(const std::string& newDisplay)
{
    S_VARS->activeDisplay = newDisplay;
    S_VARS->activeView = S_VARS->config->getDefaultView(S_VARS->activeDisplay.c_str());
    S_VARS->retrieveViews();

    updateProcessors();
}

void CMS::setActiveView(const std::string& newView)
{
    S_VARS->activeView = newView;
    updateProcessors();
}

void CMS::setActiveLook(const std::string& newLook)
{
    S_VARS->activeLook = newLook;
    updateProcessors();
}

float CMS::getExposure()
{
    return S_VARS->exposure;
}

void CMS::setExposure(float newExposure)
{
    S_VARS->exposure = newExposure;
}

void CMS::updateProcessors()
{
    // View transforms aren't needed in CLI mode
    if (CLI::hasCommands())
        return;

    try
    {
        // Config
        OCIO::ConstConfigRcPtr config = getConfig();

        // Create group transform
        S_VARS->groupTransform = OCIO::GroupTransform::Create();

        // Look transform

        std::string lookName = S_VARS->activeLook;
        std::string lookOutput = "";
        bool useLook = (!lookName.empty()) && (lookName != "None");

        if (useLook)
        {
            const char* lookOutputCC = OCIO::LookTransform::GetLooksResultColorSpace(config, config->getCurrentContext(), lookName.c_str());

            if (lookOutputCC != nullptr && lookOutputCC[0] != 0)
            {
                lookOutput = lookOutputCC;

                OCIO::LookTransformRcPtr lookTransform = OCIO::LookTransform::Create();
                lookTransform->setSrc(OCIO::ROLE_SCENE_LINEAR);
                lookTransform->setDst(lookOutputCC);
                lookTransform->setLooks(lookName.c_str());
                S_VARS->groupTransform->appendTransform(lookTransform);
            }
            else
            {
                // For empty looks, no output color space is returned.
                useLook = false;
            }
        }

        // Display view transform
        OCIO::DisplayViewTransformRcPtr displayViewTransform = OCIO::DisplayViewTransform::Create();
        displayViewTransform->setSrc(useLook ? lookOutput.c_str() : OCIO::ROLE_SCENE_LINEAR);
        displayViewTransform->setLooksBypass(useLook);
        displayViewTransform->setView(S_VARS->activeView.c_str());
        displayViewTransform->setDisplay(S_VARS->activeDisplay.c_str());
        S_VARS->groupTransform->appendTransform(displayViewTransform);

        // Get processors
        S_VARS->processor = S_VARS->config->getProcessor(S_VARS->groupTransform);
        S_VARS->cpuProcessor = S_VARS->processor->getDefaultCPUProcessor();
        S_VARS->gpuProcessor = S_VARS->processor->getDefaultGPUProcessor();

        if (USE_GPU)
        {
            // Prepare shader description
            OCIO::GpuShaderDescRcPtr shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
            shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_1_3);
            shaderDesc->setFunctionName("OCIODisplay");
            shaderDesc->setResourcePrefix("ocio_");

            // Extract shader info into shaderDesc
            S_VARS->gpuProcessor->extractGpuShaderInfo(shaderDesc);

            // Recreate the OCIO shader
            S_VARS->shader = std::make_shared<OcioShader>(shaderDesc);
        }

        S_STATE.setOk();
    } catch (const std::exception& e)
    {
        S_STATE.setError(printErr(__FUNCTION__, e.what()));
    }
}

bool CMS::ok()
{
    return S_STATE.ok();
}

std::string CMS::getError()
{
    return S_STATE.getError();
}

OCIO::ConstCPUProcessorRcPtr CMS::getCpuProcessor()
{
    if (ok())
        return S_VARS->cpuProcessor;
    return nullptr;
}

OCIO::ConstGPUProcessorRcPtr CMS::getGpuProcessor()
{
    if (ok())
        return S_VARS->gpuProcessor;
    return nullptr;
}

std::shared_ptr<OcioShader> CMS::getShader()
{
    return S_VARS->shader;
}

bool CMS::usingGPU()
{
    return USE_GPU;
}

std::string getColorSpaceDesc(OCIO::ConstConfigRcPtr config, const std::string& colorSpace)
{
    OCIO::ConstColorSpaceRcPtr cs = config->getColorSpace(colorSpace.c_str());
    if (cs.get() == nullptr)
        return "";
    return cs->getDescription();
}
