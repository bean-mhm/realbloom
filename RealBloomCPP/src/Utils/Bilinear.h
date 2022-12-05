#pragma once

#include <iostream>
#include <math.h>

namespace Bilinear
{
    struct Result
    {
        float topLeftWeight;
        float topRightWeight;
        float bottomLeftWeight;
        float bottomRightWeight;

        int topLeftPos[2];
        int topRightPos[2];
        int bottomLeftPos[2];
        int bottomRightPos[2];

        float util_alongX;
        float util_alongY;
    };

    void calculate(float px, float py, Result& outResult);
}