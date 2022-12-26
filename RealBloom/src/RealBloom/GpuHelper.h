#pragma once

#include <string>
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

typedef std::function<void(std::string inpFilename, std::string outFilename, std::ifstream& inpFile, std::ofstream& outFile)> GpuHelperOperation;

class GpuHelper
{
private:
    std::string m_inpFilename;
    std::string m_outFilename;

    STARTUPINFOA m_startupInfo{ };
    PROCESS_INFORMATION m_processInfo{ };
    bool m_hasHandles = false;

public:
    GpuHelper();

    const std::string& getInpFilename();
    const std::string& getOutFilename();

    void run();
    bool isRunning();
    void kill();
    bool outputCreated();
    void cleanUp(bool deleteFiles);
};
