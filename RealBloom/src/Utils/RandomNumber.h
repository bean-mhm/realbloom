#pragma once

#include <random>

class RandomNumber
{
private:
    static std::random_device s_randomDevice; // for seeding
    static std::mt19937 s_engine; // create engine and seed it
    static std::uniform_int_distribution<int> s_distInt; // distribution for integers
    static std::uniform_real_distribution<double> s_distReal; // .. for real numbers

public:
    static int nextInt(); // 1 to INT_MAX
    static float nextFloat(); // 0 to 1
    static double nextDouble(); // 0 to 1
};