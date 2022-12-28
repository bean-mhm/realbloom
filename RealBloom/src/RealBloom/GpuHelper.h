#pragma once

#include <string>
#include <chrono>
#include <thread>
#include <functional>
#include <filesystem>
#include <iostream>
#include <fstream>

#include "../Utils/RandomNumber.h"
#include "../Utils/Misc.h"
#include "../Async.h"

enum class GpuHelperOperationType
{
    Diffraction = 0,
    Dispersion = 1,
    ConvNaive = 2,
    ConvFFT = 3
};

// Is this Java? Perhaps not long enough of a name.
std::string strFromGpuHelperOperationType(GpuHelperOperationType opType);

constexpr uint32_t GPU_HELPER_FILE_TIMEOUT = 5000;
constexpr bool GPU_HELPER_DELETE_TEMP = true;

class GpuHelper
{
private:
    uint64_t m_randomNumber;

    std::string m_inpFilename;
    std::string m_outFilename;

    STARTUPINFOA m_startupInfo{ };
    PROCESS_INFORMATION m_processInfo{ };
    bool m_hasHandles = false;

public:
    GpuHelper();

    uint64_t getRandomNumber();
    const std::string& getInpFilename();
    const std::string& getOutFilename();

    void run();
    void waitForOutput(bool* pMustCancel);
    bool isRunning();
    void kill();
    void cleanUp();
};
