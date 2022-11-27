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
        double contrast = 0;
        double exposure = 0;
        bool grayscale = false;
    };

    class DiffractionPattern
    {
    private:
        DiffractionPatternParams m_params;

        CmImage* m_imageAperture = nullptr;
        CmImage* m_imageDiffPattern = nullptr;

        Array2D<double> m_fftMag[3];
        double m_maxMag[3]{ EPSILON, EPSILON, EPSILON };
        bool m_hasFftData = false;

        bool m_success = false;
        std::string m_error = "";

    public:
        DiffractionPattern();
        DiffractionPatternParams* getParams();

        void setApertureImage(CmImage* image);
        void setDiffPatternImage(CmImage* image);

        void compute();
        void render();

        bool hasFftData() const;
        bool success() const;
        std::string getError() const;
    };

}