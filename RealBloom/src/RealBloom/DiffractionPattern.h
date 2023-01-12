#pragma once

#include <vector>
#include <string>
#include <math.h>
#include <complex>

#include "pocketfft/pocketfft_hdronly.h"

#include "../ColorManagement/CmImage.h"

#include "../Utils/Array2D.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Status.h"
#include "../Utils/Misc.h"

#include "../Async.h"

namespace RealBloom
{
    struct DiffractionPatternParams
    {
        bool grayscale = false;
    };

    class DiffractionPattern
    {
    private:
        BaseStatus m_status;
        DiffractionPatternParams m_params;

        CmImage* m_imgAperture = nullptr;
        CmImage* m_imgDiffPattern = nullptr;

    public:
        DiffractionPattern();
        DiffractionPatternParams* getParams();

        void setImgAperture(CmImage* image);
        void setImgDiffPattern(CmImage* image);

        void compute();

        const BaseStatus& getStatus() const;
    };

}
