#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <complex>
#include <cmath>

#include "pocketfft/pocketfft_hdronly.h"

#include "ModuleHelpers.h"

#include "../ColorManagement/CmImage.h"

#include "../Utils/ImageTransform.h"
#include "../Utils/Array2D.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Status.h"
#include "../Utils/Misc.h"

#include "../Async.h"

namespace RealBloom
{

    struct DiffractionParams
    {
        ImageTransformParams inputTransformParams;
    };

    class Diffraction
    {
    private:
        BaseStatus m_status;
        DiffractionParams m_params;

        CmImage m_imgInputSrc;
        CmImage* m_imgInput = nullptr;

        CmImage* m_imgDiff = nullptr;

    public:
        Diffraction();
        DiffractionParams* getParams();

        CmImage* getImgInputSrc();
        void setImgInput(CmImage* image);

        void setImgDiff(CmImage* image);

        void previewInput(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void compute();

        const BaseStatus& getStatus() const;
    };

}
