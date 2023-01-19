#include "Bilinear.h"

inline float getT(float value, float start)
{
    return value - start;
}

void Bilinear::calc(float px, float py)
{
    topLeftPos[0] = (int)floorf(px - 0.5f);
    topLeftPos[1] = (int)floorf(py - 0.5f);

    bottomRightPos[0] = topLeftPos[0] + 1;
    bottomRightPos[1] = topLeftPos[1] + 1;

    topRightPos[0] = bottomRightPos[0];
    topRightPos[1] = topLeftPos[1];

    bottomLeftPos[0] = topLeftPos[0];
    bottomLeftPos[1] = bottomRightPos[1];

    alongX = getT(px, topLeftPos[0] + 0.5f);
    alongY = getT(py, topLeftPos[1] + 0.5f);

    topLeftWeight = (1.0f - alongX) * (1.0f - alongY);
    topRightWeight = (alongX) * (1.0f - alongY);
    bottomLeftWeight = (1.0f - alongX) * (alongY);
    bottomRightWeight = (alongX) * (alongY);
}
