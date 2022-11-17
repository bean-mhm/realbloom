#pragma once

#include <vector>
#include <string>

#include <fftw/fftw3.h>
#include <math.h>

#include "../Utils/NumberHelpers.h"
#include "../Utils/RandomNumber.h"

namespace RealBloom
{
    struct DiffractionPatternParams
    {
        uint32_t width = 0;
        uint32_t height = 0;
        double contrast = 0;
        double exposure = 0;
        bool grayscale = false;
    };

    class DiffractionPattern
    {
    private:
        DiffractionPatternParams m_params;

        std::vector<double> m_rawR;
        std::vector<double> m_rawG;
        std::vector<double> m_rawB;

        bool m_hasRawData;
        double m_maxMag[3];
        bool m_success;
        std::string m_error;

    public:
        DiffractionPattern();

        DiffractionPatternParams* getParams();
        void compute(float* buffer);
        bool getRgbaOutput(std::vector<float>& outBuffer);

        bool hasRawData() const;
        bool success();
        std::string getError();
    };

}