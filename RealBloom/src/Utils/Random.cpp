#include "Random.h"

std::random_device Random::S_RANDOM_DEVICE;
std::mt19937 Random::S_ENGINE(Random::S_RANDOM_DEVICE());

std::uniform_int_distribution<int32_t> Random::S_DIST_I32(
    std::numeric_limits<int32_t>::min(),
    std::numeric_limits<int32_t>::max());

std::uniform_int_distribution<uint32_t> Random::S_DIST_U32(
    std::numeric_limits<uint32_t>::min(),
    std::numeric_limits<uint32_t>::max());

std::uniform_int_distribution<int64_t> Random::S_DIST_I64(
    std::numeric_limits<int64_t>::min(),
    std::numeric_limits<int64_t>::max());

std::uniform_int_distribution<uint64_t> Random::S_DIST_U64(
    std::numeric_limits<uint64_t>::min(),
    std::numeric_limits<uint64_t>::max());

std::uniform_real_distribution<float> Random::S_DIST_FLOAT(
    0.0f,
    1.0f);

std::uniform_real_distribution<double> Random::S_DIST_DOUBLE(
    0.0,
    1.0);

int32_t Random::nextI32()
{
    return S_DIST_I32(S_ENGINE);
}

int32_t Random::nextI32(int32_t min, int32_t max)
{
    int32_t count = (max - min) + 1;
    return min + (((nextI32() % count) + count) % count);
}

uint32_t Random::nextU32()
{
    return S_DIST_U32(S_ENGINE);
}

uint32_t Random::nextU32(uint32_t min, uint32_t max)
{
    uint32_t count = (max - min) + 1;
    return min + (((nextU32() % count) + count) % count);
}

int64_t Random::nextI64()
{
    return S_DIST_I64(S_ENGINE);
}

int64_t Random::nextI64(int64_t min, int64_t max)
{
    int64_t count = (max - min) + 1;
    return min + (((nextI64() % count) + count) % count);
}

uint64_t Random::nextU64()
{
    return S_DIST_U64(S_ENGINE);
}

uint64_t Random::nextU64(uint64_t min, uint64_t max)
{
    uint64_t count = (max - min) + 1;
    return min + (((nextU64() % count) + count) % count);
}

float Random::nextFloat()
{
    return S_DIST_FLOAT(S_ENGINE);
}

float Random::nextFloat(float min, float max)
{
    return (double)min + (nextDouble() * (double)(max - min));
}

double Random::nextDouble()
{
    return S_DIST_DOUBLE(S_ENGINE);
}

double Random::nextDouble(double min, double max)
{
    return min + (nextDouble() * (max - min));
}
