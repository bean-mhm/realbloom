#pragma once

#include <random>

class Random
{
private:
    // Engine
    static std::random_device s_randomDevice;
    static std::mt19937 s_engine;

    // Distributors
    static std::uniform_int_distribution<int32_t> s_distI32;
    static std::uniform_int_distribution<uint32_t> s_distU32;
    static std::uniform_int_distribution<int64_t> s_distI64;
    static std::uniform_int_distribution<uint64_t> s_distU64;
    static std::uniform_real_distribution<float> s_distFloat;
    static std::uniform_real_distribution<double> s_distDouble;

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
