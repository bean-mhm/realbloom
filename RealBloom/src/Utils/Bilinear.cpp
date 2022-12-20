#include "Bilinear.h"

namespace Bilinear
{

    inline float getT(float value, float start)
    {
        return value - start;
    }

    void calculate(float px, float py, Result& outResult)
    {
        outResult.topLeftPos[0] = (int)floorf(px - 0.5f);
        outResult.topLeftPos[1] = (int)floorf(py - 0.5f);

        outResult.bottomRightPos[0] = outResult.topLeftPos[0] + 1;
        outResult.bottomRightPos[1] = outResult.topLeftPos[1] + 1;

        outResult.topRightPos[0] = outResult.bottomRightPos[0];
        outResult.topRightPos[1] = outResult.topLeftPos[1];

        outResult.bottomLeftPos[0] = outResult.topLeftPos[0];
        outResult.bottomLeftPos[1] = outResult.bottomRightPos[1];

        outResult.util_alongX = getT(px, outResult.topLeftPos[0] + 0.5f);
        outResult.util_alongY = getT(py, outResult.topLeftPos[1] + 0.5f);

        outResult.topLeftWeight = (1.0f - outResult.util_alongX) * (1.0f - outResult.util_alongY);
        outResult.topRightWeight = (outResult.util_alongX) * (1.0f - outResult.util_alongY);
        outResult.bottomLeftWeight = (1.0f - outResult.util_alongX) * (outResult.util_alongY);
        outResult.bottomRightWeight = (outResult.util_alongX) * (outResult.util_alongY);
    }

}