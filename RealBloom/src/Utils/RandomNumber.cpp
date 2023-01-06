#include "RandomNumber.h"

std::random_device RandomNumber::s_randomDevice;
std::mt19937 RandomNumber::s_engine(RandomNumber::s_randomDevice());

std::uniform_int_distribution<int32_t> RandomNumber::s_distI32(
    std::numeric_limits<int32_t>::min(),
    std::numeric_limits<int32_t>::max());

std::uniform_int_distribution<uint32_t> RandomNumber::s_distU32(
    std::numeric_limits<uint32_t>::min(),
    std::numeric_limits<uint32_t>::max());

std::uniform_int_distribution<int64_t> RandomNumber::s_distI64(
    std::numeric_limits<int64_t>::min(),
    std::numeric_limits<int64_t>::max());

std::uniform_int_distribution<uint64_t> RandomNumber::s_distU64(
    std::numeric_limits<uint64_t>::min(),
    std::numeric_limits<uint64_t>::max());

std::uniform_real_distribution<float> RandomNumber::s_distFloat(
    0.0f,
    1.0f);

std::uniform_real_distribution<double> RandomNumber::s_distDouble(
    0.0,
    1.0);

int32_t RandomNumber::nextI32()
{
    return s_distI32(s_engine);
}

uint32_t RandomNumber::nextU32()
{
    return s_distU32(s_engine);
}

int64_t RandomNumber::nextI64()
{
    return s_distI64(s_engine);
}

uint64_t RandomNumber::nextU64()
{
    return s_distU64(s_engine);
}

int32_t RandomNumber::nextI32(int32_t min, int32_t max)
{
    double v = (double)min + floor(nextDouble() * (double)((max - min) + 1));
    return (int32_t)fmin(fmax(v, min), max);
}

uint32_t RandomNumber::nextU32(uint32_t min, uint32_t max)
{
    double v = (double)min + floor(nextDouble() * (double)((max - min) + 1));
    return (uint32_t)fmin(fmax(v, min), max);
}

int64_t RandomNumber::nextI64(int64_t min, int64_t max)
{
    double v = (double)min + floor(nextDouble() * (double)((max - min) + 1));
    return (int64_t)fmin(fmax(v, min), max);
}

uint64_t RandomNumber::nextU64(uint64_t min, uint64_t max)
{
    double v = (double)min + floor(nextDouble() * (double)((max - min) + 1));
    return (uint64_t)fmin(fmax(v, min), max);
}

float RandomNumber::nextFloat()
{
    return s_distFloat(s_engine);
}

double RandomNumber::nextDouble()
{
    return s_distDouble(s_engine);
}

float RandomNumber::nextFloat(float min, float max)
{
    return (double)min + (nextDouble() * (double)(max - min));
}

double RandomNumber::nextDouble(double min, double max)
{
    return min + (nextDouble() * (max - min));
}
