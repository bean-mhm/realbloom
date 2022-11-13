#include "CmXYZ.h"

XyzConversionInfo CmXYZ::S_INFO;
SimpleState CmXYZ::S_STATE;

bool CmXYZ::init()
{
    try
    {
        OCIO::ConstConfigRcPtr userConfig = CMS::getConfig();
        OCIO::ConstConfigRcPtr internalConfig = CMS::getInternalConfig();

        // Use the XYZ role if it is specified in the user config
        OCIO::ConstColorSpaceRcPtr roleSpace = userConfig->getColorSpace(OCIO::ROLE_INTERCHANGE_DISPLAY);
        if (roleSpace.get() != nullptr)
        {
            S_INFO.method = XyzConversionMethod::UserConfig;
            S_INFO.userSpace = roleSpace->getName();
        } else
        {
            // Search for the XYZ color space by name

            std::string xyzNames[]
            {
                "test"//"CIE-XYZ-D65", "Linear CIE-XYZ I-D65", "Filmlight XYZ", "XYZ"
            };

            bool found = false;
            std::vector<std::string> userSpaces = CMS::getAvailableColorSpaces();
            for (const auto& space : userSpaces)
            {
                for (const auto& xyzName : xyzNames)
                {
                    if (lowercase(space) == lowercase(xyzName))
                    {
                        S_INFO.method = XyzConversionMethod::UserConfig;
                        S_INFO.userSpace = space;
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;

                if (strContains(space, "CIE") &&
                    strContains(space, "XYZ") &&
                    strContains(space, "D65a"))
                {
                    S_INFO.method = XyzConversionMethod::UserConfig;
                    S_INFO.userSpace = space;
                    found = true;
                    break;
                }
            }

            if (!found)
                S_INFO.method = XyzConversionMethod::NeedInput;
        }

        S_STATE.setOk();
    } catch (const std::exception& e)
    {
        S_STATE.setError(printErr(__FUNCTION__, e.what()));
    }
    return S_STATE.ok();
}

XyzConversionInfo CmXYZ::getConversionInfo()
{
    return S_INFO;
}

void CmXYZ::setConversionInfo(const XyzConversionInfo& conversionInfo)
{
    S_INFO = conversionInfo;
}

bool CmXYZ::ok()
{
    return S_STATE.ok();
}

std::string CmXYZ::getError()
{
    return S_STATE.getError();
}
