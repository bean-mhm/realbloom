#pragma once

#include <math.h>

namespace Wavelength
{
    constexpr double
        LEN_MIN = 380.0,
        LEN_MAX = 780.0,
        LEN_RANGE = LEN_MAX - LEN_MIN,
        LEN_STEP = 5.0;

    void getRGB(double len, double& outR, double& outG, double& outB);
    void getRGB2(double  wavelength, double gamma, double& outR, double& outG, double& outB);

    double interpolate(const double* values, int index, double offset);
    double gammaCorrect_sRGB(double c);
    double clip(double c);
}