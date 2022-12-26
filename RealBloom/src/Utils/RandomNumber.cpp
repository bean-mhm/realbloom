#include "RandomNumber.h"

std::random_device RandomNumber::s_randomDevice;
std::mt19937 RandomNumber::s_engine(RandomNumber::s_randomDevice());

std::uniform_int_distribution<int32_t> RandomNumber::s_distI32(INT_MIN, INT_MAX);
std::uniform_int_distribution<uint32_t> RandomNumber::s_distU32(0, UINT_MAX);
std::uniform_int_distribution<int64_t> RandomNumber::s_distI64(LLONG_MIN, LLONG_MAX);
std::uniform_int_distribution<uint64_t> RandomNumber::s_distU64(0, ULLONG_MAX);
std::uniform_real_distribution<float> RandomNumber::s_distFloat(0.0f, 1.0f);
std::uniform_real_distribution<double> RandomNumber::s_distDouble(0.0, 1.0);

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

float RandomNumber::nextFloat()
{
    return s_distFloat(s_engine);
}

double RandomNumber::nextDouble()
{
    return s_distDouble(s_engine);
}
