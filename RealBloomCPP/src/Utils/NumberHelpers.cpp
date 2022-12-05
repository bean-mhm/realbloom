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

    v = fminf(fmaxf(v, 0.0f), 1.0f);

    contrast *= 4.0f;
    float c =
        (contrast >= 0.0f) ?
        (contrast + 1.0f) :
        (1.0f / (1.0f - contrast));

    if (v > 0.5f)
        return powf((1.0f - v), c) * (powf(2.0f, c) / (-2.0f)) + 1.0f;
    else
        return powf(v, c) * (powf(2.0f, c) / 2.0f);

#if 0
    // Old implementation

    if (v == 0.0f)
        return 0.0f;

    if (contrast == 0.0f)
        return v;

    contrast *= 3.0f;
    v = fmaxf(v, 0.0f);

    float c = (4.0f / 5.0f) * fabsf(contrast) + 1.0f;
    return (contrast >= 0.0f) ? powf(v, c) : powf(v, 1.0f / c);
#endif
}

double contrastCurve(double v, double contrast)
{
    if (v == 0.0)
        return 0.0;

    if (contrast == 0.0)
        return v;

    v = fmin(fmax(v, 0.0), 1.0);

    contrast *= 4.0;
    double c =
        (contrast >= 0.0) ?
        (contrast + 1.0) :
        (1.0 / (1.0 - contrast));

    if (v > 0.5)
        return pow((1.0 - v), c) * (pow(2.0, c) / (-2.0)) + 1.0;
    else
        return pow(v, c) * (pow(2.0, c) / 2.0);
}

void rotatePoint(float x, float y, float pivotX, float pivotY, float angle, float& outX, float& outY)
{
    float s = sinf(angle * DEG_TO_RAD);
    float c = cosf(angle * DEG_TO_RAD);

    outX = ((x - pivotX) * c - (y - pivotY) * s) + pivotX;
    outY = ((x - pivotX) * s + (y - pivotY) * c) + pivotY;
}