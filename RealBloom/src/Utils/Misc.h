#pragma once

#include <iostream>
#include <iomanip>

#include <thread>
#include <chrono>
#include <string>
#include <stdint.h>
#include <math.h>
#include <filesystem>
#include <algorithm>
#include <vector>

#include "StringUtils.h"
#include "StreamUtils.h"

#define NOMINMAX
#include <Windows.h>

#define DELPTR(PTR) if (PTR) { delete PTR; PTR = nullptr; }
#define DELARR(PTR) if (PTR) { delete[] PTR; PTR = nullptr; }

#define AS_BYTES(X) reinterpret_cast<char*>(&(X))
#define PTR_AS_BYTES(X) reinterpret_cast<char*>(X)

std::string printErr(
    const std::string& source,
    const std::string& stage,
    const std::string& message,
    bool printAnyway = false);

std::string printErr(
    const std::string& source,
    const std::string& message,
    bool printAnyway = false);

void disablePrintErr();

template <typename T>
void clearVector(std::vector<T>& v)
{
    v.clear();
    std::vector<T>().swap(v);
}

inline void threadJoin(std::thread* t)
{
    if (t)
        if (t->joinable())
            t->join();
}

uint32_t getMaxNumThreads();
uint32_t getDefNumThreads();

uint32_t getElapsedMs(std::chrono::system_clock::time_point startTime);

HANDLE createMutex(const std::string& name);
HANDLE openMutex(const std::string& name);
void waitForMutex(HANDLE hMutex);
void releaseMutex(HANDLE hMutex);
void closeMutex(HANDLE hMutex);

std::string getPathSeparator();
std::string getExecDir();
std::string getLocalPath(const std::string& path);

void killProcess(PROCESS_INFORMATION pi);
bool processIsRunning(PROCESS_INFORMATION pi);
bool deleteFile(const std::string& filename);
void getTempDirectory(std::string& outDir);
void openURL(std::string url);

class SimpleState
{
private:
    bool m_ok = true;
    std::string m_error = "";

public:
    void setError(const std::string& message);
    void setOk();

    bool ok() const;
    std::string getError() const;
};
