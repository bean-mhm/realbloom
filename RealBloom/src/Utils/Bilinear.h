#pragma once

#include <iostream>
#include <cmath>

struct Bilinear
{
    int topLeftPos[2]{ 0, 0 };
    int topRightPos[2]{ 0, 0 };
    int bottomLeftPos[2]{ 0, 0 };
    int bottomRightPos[2]{ 0, 0 };

    float topLeftWeight = 0.0f;
    float topRightWeight = 0.0f;
    float bottomLeftWeight = 0.0f;
    float bottomRightWeight = 0.0f;

    float alongX = 0;
    float alongY = 0;

    void calc(float px, float py);

};
