#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <stdint.h>

bool strContains(const std::string& source, const std::string& substring);
std::string strList(const std::vector<std::string>& list, const std::string& separator);
std::string strRightPadding(const std::string& s, size_t length);
std::string strWordWrap(const std::string& s, size_t length, size_t leftPadding = 0);

// Examples: "6h 9m 42s", "10.7s"
std::string strFromDuration(float seconds);

// Examples: "06:09:42", "00:00:10"
std::string strFromElapsed(float seconds);

std::string strFromSize(uint64_t sizeBytes);
std::string strFromBigNumber(uint64_t bigNumber);

template< typename T >
std::string toHexStr(T i)
{
    std::stringstream stream;
    stream << "0x"
        << std::setfill('0') << std::setw(sizeof(T) * 2)
        << std::hex << i;
    return stream.str();
}

template<typename ... Args>
std::string strFormat(const std::string& format, Args ... args)
{
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    if (size_s <= 0)
    {
        //throw std::runtime_error("Error during formatting.");
        return "[formatStr] Error";
    }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

template <typename T>
std::basic_string<T> strLowercase(const std::basic_string<T>& s)
{
    std::basic_string<T> s2 = s;
    std::transform(s2.begin(), s2.end(), s2.begin(), tolower);
    return s2;
}

template <typename T>
std::basic_string<T> strUppercase(const std::basic_string<T>& s)
{
    std::basic_string<T> s2 = s;
    std::transform(s2.begin(), s2.end(), s2.begin(), toupper);
    return s2;
}