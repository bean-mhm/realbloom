#pragma once

#include <vector>
#include <string>
#include <complex>
#include <cmath>

#include "pocketfft/pocketfft_hdronly.h"

#include "../ColorManagement/CmImage.h"

#include "../Utils/ImageTransform.h"
#include "../Utils/Array2D.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Status.h"
#include "../Utils/Misc.h"

#include "../Async.h"

namespace RealBloom
{

    class Diffraction
    {
    private:
        BaseStatus m_status;
        ImageTransformParams m_inputTransformParams;

        CmImage* m_imgInput = nullptr;
        CmImage* m_imgDiff = nullptr;

        CmImage m_imgInputSrc;

    public:
        Diffraction();
        ImageTransformParams* getInputTransformParams();

        void setImgInput(CmImage* image);
        void setImgDiff(CmImage* image);

        CmImage* getImgInputSrc();

        void previewInput(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void compute();

        const BaseStatus& getStatus() const;
    };

}