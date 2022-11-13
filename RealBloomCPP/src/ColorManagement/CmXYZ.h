#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

#include "CMS.h"
#include "../Utils/Misc.h"

enum class XyzConversionMethod
{
    // The user config has an XYZ color space
    UserConfig,

    // The user config doesn't have an XYZ space,
    // so we convert from XYZ to a common space
    // in the internal and user configs, then to
    // the working space
    CommonSpace,

    // A method couldn't be automatically chosen,
    // and we need the user to choose either an
    // XYZ color space or a common space
    NeedInput
};

struct XyzConversionInfo
{
    XyzConversionMethod method = XyzConversionMethod::UserConfig;
    std::string userSpace = "";
    std::string commonInternal = "";
    std::string commonUser = "";
};

class CmXYZ
{
private:
    static XyzConversionInfo S_INFO;
    static SimpleState S_STATE;

public:
    CmXYZ() = delete;
    CmXYZ(const CmXYZ&) = delete;
    CmXYZ& operator= (const CmXYZ&) = delete;

    static bool init();
    static XyzConversionInfo getConversionInfo();
    static void setConversionInfo(const XyzConversionInfo& conversionInfo);

    static bool ok();
    static std::string getError();
};
