#pragma once

#include <iostream>
#include <iomanip>

#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <cmath>

#include "StringUtils.h"
#include "StreamUtils.h"

#define NOMINMAX
#include <Windows.h>

#define DELPTR(PTR) if (PTR) { delete PTR; PTR = nullptr; }
#define DELARR(PTR) if (PTR) { delete[] PTR; PTR = nullptr; }

#define AS_BYTES(X) reinterpret_cast<char*>(&(X))
#define PTR_AS_BYTES(X) reinterpret_cast<char*>(X)

constexpr uint32_t WAIT_TIMESTEP_SHORT = 10;

std::string makeError(
    const std::string& source,
    const std::string& stage,
    const std::string& message,
    bool print = false);

void printError(
    const std::string& source,
    const std::string& stage,
    const std::string& message);

void printWarning(
    const std::string& source,
    const std::string& stage,
    const std::string& message);

void printInfo(
    const std::string& source,
    const std::string& stage,
    const std::string& message);

void setPrintHandler(std::function<void(std::string)> handler);

template <typename T>
void clearVector(std::vector<T>& v)
{
    v.clear();
    std::vector<T>().swap(v);
}

template <typename T>
void insertContents(std::vector<T>& a, const std::vector<T>& b)
{
    a.reserve(a.size() + b.size());
    a.insert(a.end(), b.begin(), b.end());
}

template <typename T>
int findIndex(const std::vector<T>& vector, const T& value)
{
    for (size_t i = 0; i < vector.size(); i++)
        if (value == vector[i])
        {
            return i;
        }
    return -1;
}

template <typename T>
bool contains(const std::vector<T>& vector, const T& value)
{
    return findIndex(vector, value) >= 0;
}

inline void threadJoin(std::thread* t)
{
    if (t)
        if (t->joinable())
            t->join();
}

inline void threadJoin(std::jthread* t)
{
    if (t)
        if (t->joinable())
            t->join();
}

uint32_t getMaxNumThreads();
uint32_t getDefNumThreads();

float getElapsedMs(std::chrono::system_clock::time_point startTime);
float getElapsedMs(std::chrono::system_clock::time_point startTime, std::chrono::system_clock::time_point endTime);

HANDLE createMutex(const std::string& name);
HANDLE openMutex(const std::string& name);
void waitForMutex(HANDLE hMutex);
void releaseMutex(HANDLE hMutex);
void closeMutex(HANDLE& hMutex);

const std::string& getPathSeparator();
const std::string& getExecDir();
const std::string& getTempDirectory();

std::string getLocalPath(const std::string& path);
std::string getFileExtension(const std::string& filename);
bool deleteFile(const std::string& filename);

void killProcess(PROCESS_INFORMATION pi);
bool processIsRunning(PROCESS_INFORMATION pi);
void openURL(std::string url);
