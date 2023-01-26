#pragma once

#include <iostream>
#include <cstdint>

#include "Misc.h"

void stmCheck(std::ios_base& stream, const std::string& source, const std::string& stage);

std::string stmReadString(std::istream& stream);
void stmWriteString(std::ostream& stream, const std::string& s);

template <typename T>
T stmReadScalar(std::istream& stream)
{
    if (stream.fail())
        return 0;

    T v;
    stream.read(reinterpret_cast<char*>(&v), sizeof(T));

    return v;
}

template <typename T>
void stmWriteScalar(std::ostream& stream, T v)
{
    if (stream.fail())
        return;
    stream.write(reinterpret_cast<char*>(&v), sizeof(T));
}

template <typename T>
void stmReadVector(std::istream& stream, std::vector<T>& outVector)
{
    // Clear
    outVector.clear();
    std::vector<T>().swap(outVector);

    if (stream.fail())
        return;

    // Read the size
    uint32_t size = 0;
    stream.read(reinterpret_cast<char*>(&size), sizeof(size));

    if (stream.fail()) return;
    if (size < 1) return;

    // Resize
    outVector.resize(size);

    // Read the content
    stream.read(reinterpret_cast<char*>(outVector.data()), size * sizeof(T));
}

template <typename T>
void stmWriteVector(std::ostream& stream, std::vector<T>& vector)
{
    if (stream.fail())
        return;

    // Write the size
    uint32_t size = vector.size();
    stream.write(reinterpret_cast<char*>(&size), sizeof(size));

    // Write the content
    if (size > 0)
        stream.write(reinterpret_cast<char*>(vector.data()), size * sizeof(T));
}
