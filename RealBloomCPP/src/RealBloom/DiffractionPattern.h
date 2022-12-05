#pragma once

#include <vector>
#include <string>
#include <math.h>
#include <complex>

#include "pocketfft/pocketfft_hdronly.h"

#include "../ColorManagement/CMImage.h"
#include "../Utils/Array2D.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/RandomNumber.h"
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
        DiffractionPatternParams m_params;

        CmImage* m_imgAperture = nullptr;
        CmImage* m_imgDiffPattern = nullptr;

        bool m_success = false;
        std::string m_error = "";

    public:
        DiffractionPattern();
        DiffractionPatternParams* getParams();

        void setImgAperture(CmImage* image);
        void setImgDiffPattern(CmImage* image);

        void compute();

        bool success() const;
        std::string getError() const;
    };

}