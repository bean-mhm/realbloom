#pragma once

#include <vector>
#include <string>
#include <math.h>
#include <complex>

#include "pocketfft/pocketfft_hdronly.h"

#include "../Utils/Array2D.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/RandomNumber.h"

namespace RealBloom
{
    struct DiffractionPatternParams
    {
        uint32_t inputWidth = 0;
        uint32_t inputHeight = 0;
        double contrast = 0;
        double exposure = 0;
        bool grayscale = false;
    };

    class DiffractionPattern
    {
    private:
        DiffractionPatternParams m_params;

        Array2D<double> m_fftMag[3];
        double m_maxMag[3]{ EPSILON, EPSILON, EPSILON };
        bool m_hasFftData = false;

        bool m_success = false;
        std::string m_error = "";

    public:
        DiffractionPattern();

        DiffractionPatternParams* getParams();
        void compute(float* buffer);
        uint32_t getOutputWidth() const;
        uint32_t getOutputHeight() const;
        bool getRgbaOutput(std::vector<float>& outBuffer) const;

        bool hasFftData() const;
        bool success();
        std::string getError();
    };

}