#include "CmXYZ.h"

XyzConversionInfo CmXYZ::S_INFO;
BaseStatus CmXYZ::S_STATUS;

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
    }
    catch (const std::exception& e)
    {
        S_STATUS.setError(makeError(__FUNCTION__, "", e.what(), true));
    }

    return S_STATUS.isOK();
}

XyzConversionInfo CmXYZ::getConversionInfo()
{
    return S_INFO;
}

void CmXYZ::setConversionInfo(const XyzConversionInfo& conversionInfo)
{
    S_STATUS.reset();
    S_INFO = conversionInfo;
}

const BaseStatus& CmXYZ::getStatus()
{
    return S_STATUS;
}

void CmXYZ::ensureOK()
{
    if (!S_STATUS.isOK())
        throw std::exception(strFormat("CmXYZ failure: %s", S_STATUS.getError().c_str()).c_str());
}
