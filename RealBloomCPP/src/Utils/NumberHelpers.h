#pragma once

#include <stdint.h>
#include <math.h>
#include <numbers>

constexpr float DEG_TO_RAD = (float)std::numbers::pi / 180.0f;
constexpr float EPSILON = 0.000001f;

inline bool areEqual(float a, float b, float epsilon = EPSILON)
{
    return (b > (a - epsilon)) && (b < (a + epsilon));
}

inline float squared(float& v)
{
    return v * v;
}

inline float clamp(float v, float min, float max)
{
    return fminf(fmaxf(v, min), max);
}

inline float getMagnitude(float a, float b)
{
    return sqrtf(a * a + b * b);
}

inline double getMagnitude(double a, double b)
{
    return sqrt(a * a + b * b);
}

inline float getPhase(float x, float y)
{
    return atan2f(y, x);
}

inline double getPhase(double x, double y)
{
    return atan2(y, x);
}

inline float getDistance(float x1, float y1, float x2, float y2)
{
    return sqrtf(powf(x2 - x1, 2) + powf(y2 - y1, 2));
}


inline float lerp(float a, float b, float t)
{
    return a + ((b - a) * t);
}

inline uint32_t upperPowerOf2(uint32_t v)
{
    return (uint32_t)floor(pow(2, ceil(log(v) / log(2))));
}

inline float rgbToGrayscale(float r, float g, float b)
{
    return (r * 0.2126f) + (g * 0.7152f) + (b * 0.0722f);
}

inline double rgbToGrayscale(double r, double g, double b)
{
    return (r * 0.2126) + (g * 0.7152) + (b * 0.0722);
}

inline float softThreshold(float v, float threshold, float knee)
{
    return knee > 0 ? fminf((v - threshold) / knee, 1.0f) : 1.0f;
}

inline bool checkBounds(int x, int y, int w, int h)
{
    if (x < 0)
        return false;

    if (x >= w)
        return false;

    if (y < 0)
        return false;

    if (y >= h)
        return false;

    return true;
}

inline void blendAddRGB(float* colorA, uint32_t indexA, float* colorB, uint32_t indexB, float t)
{
    colorA[indexA + 0] += colorB[indexB + 0] * t;
    colorA[indexA + 1] += colorB[indexB + 1] * t;
    colorA[indexA + 2] += colorB[indexB + 2] * t;
}

inline float applyExposure(float v)
{
    return powf(2.0f, v);
}

inline double applyExposure(double v)
{
    return pow(2.0, v);
}

uint8_t doubleTo8bit(double v);
float srgbToLinear_DEPRECATED(float x);
float linearToSrgb_DEPRECATED(float x);
float contrastCurve(float v, float contrast);
double contrastCurve(double v, double contrast);
void rotatePoint(float x, float y, float pivotX, float pivotY, float angle, float& outX, float& outY);
