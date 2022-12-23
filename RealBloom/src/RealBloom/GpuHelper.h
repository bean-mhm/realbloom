#pragma once

#include <string>
#include <functional>
#include <filesystem>
#include <iostream>
#include <fstream>

#include "../Utils/Misc.h"

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
