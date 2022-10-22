#pragma once

#include <iostream>
#include <iomanip>

#include <thread>
#include <chrono>
#include <string>
#include <stdint.h>
#include <math.h>
#include <filesystem>

#define NOMINMAX
#include <Windows.h>

#define DELPTR(PTR) if (PTR) { delete PTR; PTR = nullptr; }
#define DELARR(PTR) if (PTR) { delete[] PTR; PTR = nullptr; }

#define AS_BYTES(X) reinterpret_cast<char*>(&(X))
#define PTR_AS_BYTES(X) reinterpret_cast<char*>(X)

inline void threadJoin(std::thread* t)
{
    if (t)
        if (t->joinable())
            t->join();
}

template<typename ... Args>
std::string stringFormat(const std::string& format, Args ... args)
{
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    if (size_s <= 0)
    {
        //throw std::runtime_error("Error during formatting.");
        return "Error";
    }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

// Examples: "6h 9m 42s", "10.7s"
std::string stringFromDuration(float seconds);

// Examples: "06:09:42", "00:00:10"
std::string stringFromDuration2(float seconds);

std::string stringFromSize(uint64_t sizeBytes);
std::string stringFromBigNumber(uint64_t bigNumber);

template< typename T >
std::string toHex(T i)
{
    std::stringstream stream;
    stream << "0x"
        << std::setfill('0') << std::setw(sizeof(T) * 2)
        << std::hex << i;
    return stream.str();
}

uint32_t getElapsedMs(std::chrono::system_clock::time_point startTime);

HANDLE createMutex(const std::string& name);
HANDLE openMutex(const std::string& name);
void waitForMutex(HANDLE hMutex);
void releaseMutex(HANDLE hMutex);
void closeMutex(HANDLE hMutex);

void killProcess(PROCESS_INFORMATION pi);
bool processIsRunning(PROCESS_INFORMATION pi);
bool deleteFile(const std::string& filename);
void getTempDirectory(std::string& outDir);
void openURL(std::string url);