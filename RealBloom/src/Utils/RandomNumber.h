#pragma once

#include <random>

class RandomNumber
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
    static int32_t nextI32();
    static uint32_t nextU32();
    static int64_t nextI64();
    static uint64_t nextU64();
    static float nextFloat();
    static double nextDouble();
};
