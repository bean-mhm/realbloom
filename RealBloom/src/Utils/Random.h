#pragma once

#include <random>

class Random
{
private:
    // Engine
    static std::random_device S_RANDOM_DEVICE;
    static std::mt19937 S_ENGINE;

    // Distributors
    static std::uniform_int_distribution<int32_t> S_DIST_I32;
    static std::uniform_int_distribution<uint32_t> S_DIST_U32;
    static std::uniform_int_distribution<int64_t> S_DIST_I64;
    static std::uniform_int_distribution<uint64_t> S_DIST_U64;
    static std::uniform_real_distribution<float> S_DIST_FLOAT;
    static std::uniform_real_distribution<double> S_DIST_DOUBLE;

public:
    Random() = delete;
    Random(const Random&) = delete;
    Random& operator= (const Random&) = delete;

    static int32_t nextI32();
    static int32_t nextI32(int32_t min, int32_t max);
    static uint32_t nextU32();
    static uint32_t nextU32(uint32_t min, uint32_t max);
    static int64_t nextI64();
    static int64_t nextI64(int64_t min, int64_t max);
    static uint64_t nextU64();
    static uint64_t nextU64(uint64_t min, uint64_t max);

    static float nextFloat();
    static float nextFloat(float min, float max);
    static double nextDouble();
    static double nextDouble(double min, double max);

};
