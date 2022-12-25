#include "CmXYZ.h"

XyzConversionInfo CmXYZ::S_INFO;
SimpleState CmXYZ::S_STATE;

bool CmXYZ::init()
{
    try
    {
        OCIO::ConstConfigRcPtr userConfig = CMS::getConfig();
        OCIO::ConstConfigRcPtr internalConfig = CMS::getInternalConfig();

        // Search for the CIE XYZ I-E color space by name

        std::string xyzNames[]
        {
            "Linear CIE-XYZ I-E"
        };

        bool found = false;
        std::vector<std::string> userSpaces = CMS::getAvailableColorSpaces();
        for (const auto& space : userSpaces)
        {
            for (const auto& xyzName : xyzNames)
            {
                if (strLowercase(space) == strLowercase(xyzName))
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
                strContains(space, "I-E"))
            {
                S_INFO.method = XyzConversionMethod::UserConfig;
                S_INFO.userSpace = space;
                found = true;
                break;
            }
        }

        if (!found)
            S_INFO.method = XyzConversionMethod::None;

        if (S_INFO.method == XyzConversionMethod::None)
            throw std::exception(
                "We couldn't find the proper XYZ color space in your OCIO "
                "config. If it contains the CIE XYZ I-E color space, change "
                "the method to \"User Config\" and choose the XYZ space. "
                "Otherwise, use the \"Common Space\" method, and choose a "
                "color space included in both the internal config and your "
                "custom config. Note that using D65 as the white-point will "
                "produce incorrect results."
            );

        S_STATE.setOk();
    }
    catch (const std::exception& e)
    {
        S_STATE.setError(printErr(__FUNCTION__, "", e.what()));
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
    S_STATE.setOk();
}

bool CmXYZ::ok()
{
    return S_STATE.ok();
}

std::string CmXYZ::getError()
{
    return S_STATE.getError();
}

void CmXYZ::ensureOK()
{
    if (!ok())
        throw std::exception(strFormat("CmXYZ failure: %s", getError().c_str()).c_str());
}
