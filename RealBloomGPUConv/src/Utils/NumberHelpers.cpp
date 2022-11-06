#include "NumberHelpers.h"

uint8_t doubleTo8bit(double v)
{
    v = fmax(v, 0);
    v = fmin(v, 1);
    return (uint8_t)(round(v * 255.0));
}

float srgbToLinear_DEPRECATED(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    else if (x >= 1.0f)
        return 1.0f;
    else if (x < 0.04045f)
        return x / 12.92f;
    else
        return powf((x + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb_DEPRECATED(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    else if (x >= 1.0f)
        return 1.0f;
    else if (x < 0.0031308f)
        return x * 12.92f;
    else
        return powf(x, 1.0f / 2.4f) * 1.055f - 0.055f;
}

float contrastCurve(float v, float contrast)
{
    if (v == 0.0f)
        return 0.0f;

    if (contrast == 0.0f)
        return v;

    contrast *= 3.0f;
    v = fmaxf(v, 0.0f);

    float c = (4.0f / 5.0f) * fabsf(contrast) + 1.0f;
    return (contrast >= 0.0f) ? powf(v, c) : powf(v, 1.0f / c);
}

double contrastCurve(double v, double contrast)
{
    if (v == 0.0)
        return 0.0;

    if (contrast == 0.0)
        return v;

    contrast *= 3.0;
    v = fmax(v, 0.0);

    double c = (4.0 / 5.0) * fabs(contrast) + 1.0;
    return (contrast >= 0.0) ? pow(v, c) : pow(v, 1.0 / c);
}

float intensityCurve(float v)
{
    if (v <= 0.0f)
        return 0.0f;

    if (v == 1.0f)
        return 1.0f;

    if (v < 1.0f)
        return v * v;

    return powf(10.0f, v - 1.0f);
}

double intensityCurve(double v)
{
    if (v <= 0.0)
        return 0.0;

    if (v == 1.0)
        return 1.0;

    if (v < 1.0)
        return v * v;

    return pow(10.0, v - 1.0);
}

void rotatePoint(float x, float y, float pivotX, float pivotY, float angle, float& outX, float& outY)
{
    float s = sinf(angle * DEG_TO_RAD);
    float c = cosf(angle * DEG_TO_RAD);

    outX = ((x - pivotX) * c - (y - pivotY) * s) + pivotX;
    outY = ((x - pivotX) * s + (y - pivotY) * c) + pivotY;
}