#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <cstdint>
#include <cmath>

#include "../ColorManagement/CmImage.h"

#include "../Utils/ImageTransform.h"
#include "../Utils/Misc.h"

namespace RealBloom
{

    void processInputImage(
        bool previewMode,
        ImageTransformParams& transformParams,
        CmImage& imgSrc,
        CmImage& imgPreview,
        std::vector<float>* outBuffer = nullptr,
        uint32_t* outWidth = nullptr,
        uint32_t* outHeight = nullptr);

}
