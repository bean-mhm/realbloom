#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

#include "CMS.h"

#include "../Utils/Status.h"
#include "../Utils/Misc.h"

enum class XyzConversionMethod
{
    // A method couldn't be automatically decided,
    // and we need the user to choose one.
    None,

    // Use an XYZ color space in the user config
    UserConfig,

    // Convert from XYZ to a common space in the
    // internal and user configs, then to the
    // working space
    CommonSpace
};

struct XyzConversionInfo
{
    XyzConversionMethod method = XyzConversionMethod::UserConfig;
    std::string userSpace = "";
    std::string commonInternal = "";
    std::string commonUser = "";
};

// XYZ Conversion Guide (Global)
class CmXYZ
{
public:
    CmXYZ() = delete;
    CmXYZ(const CmXYZ&) = delete;
    CmXYZ& operator= (const CmXYZ&) = delete;

    static bool init();

    static XyzConversionInfo getConversionInfo();
    static void setConversionInfo(const XyzConversionInfo& conversionInfo);

    static const BaseStatus& getStatus();
    static void ensureOK();

private:
    static XyzConversionInfo S_INFO;
    static BaseStatus S_STATUS;

};
