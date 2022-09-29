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
        uint32_t width;
        uint32_t height;
        double contrast;
        double multiplier;
        bool grayscale;
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
        DiffractionPattern(uint32_t width, uint32_t height);

        DiffractionPatternParams* getParams();
        void compute(float* buffer);
        bool getRgbaOutput(std::vector<float>& outBuffer);

        bool hasRawData() const;
        bool success();
        std::string getError();
    };

}