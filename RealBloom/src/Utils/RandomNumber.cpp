#include "RandomNumber.h"

std::random_device RandomNumber::s_randomDevice;
std::mt19937 RandomNumber::s_engine(RandomNumber::s_randomDevice());
std::uniform_int_distribution<int> RandomNumber::s_distInt(1, INT_MAX);
std::uniform_real_distribution<double> RandomNumber::s_distReal(0.0, 1.0);

int RandomNumber::nextInt()
{
    return s_distInt(s_engine);
}

float RandomNumber::nextFloat()
{
    return (float)s_distReal(s_engine);
}

double RandomNumber::nextDouble()
{
    return s_distReal(s_engine);
}
