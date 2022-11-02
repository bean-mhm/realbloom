#pragma once

#include <math.h>

namespace Wavelength
{
    constexpr double
        LEN_MIN = 380.0,
        LEN_MAX = 769.0,
        LEN_RANGE = LEN_MAX - LEN_MIN,
        LEN_STEP = 1.0;

    void getRGB(double len, double& outR, double& outG, double& outB);
}