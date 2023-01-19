#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <chrono>
#include <locale>
#include <clocale>
#include <memory>

#include "RealBloom/GpuHelper.h"
#include "RealBloom/Binary/BinaryData.h"
#include "RealBloom/Binary/BinaryConvNaiveGpu.h"
#include "RealBloom/Binary/BinaryConvFftGpu.h"
#include "RealBloom/Binary/BinaryDispGpu.h"

#include "RealBloom/ConvNaiveGpu.h"
#include "RealBloom/ConvFftGpu.h"
#include "RealBloom/DispGpu.h"

#include "Utils/OpenGL/GlContext.h"
#include "Utils/Misc.h"
#include "Config.h"

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

// Use the dedicated GPU on Dual-GPU systems
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

constexpr const char* APP_TITLE = "RealBloom GPU Helper";

typedef std::function<void(std::string inpFilename, std::string outFilename, std::ifstream& inpFile, std::ofstream& outFile)> GpuHelperOperation;

void runDisp(std::string inpFilename, std::string outFilename, std::ifstream& inpFile, std::ofstream& outFile);
void runConvFFT(std::string inpFilename, std::string outFilename, std::ifstream& inpFile, std::ofstream& outFile);
void runConvNaive(std::string inpFilename, std::string outFilename, std::ifstream& inpFile, std::ofstream& outFile);

std::ofstream logFile;

enum class LogLevel
{
    Info,
    Debug,
    Warning,
    Error,
    Fatal
};

// [TIME][LEVEL] MESSAGE
// Example: [2022-09-19 06:42:00][W] OpenGL's API sucks.
void logAdd(LogLevel level, const std::string& message)
{
    std::string levelS = " ";
    switch (level)
    {
    case LogLevel::Info:
        levelS = "I";
        break;
    case LogLevel::Debug:
        levelS = "D";
        break;
    case LogLevel::Warning:
        levelS = "W";
        break;
    case LogLevel::Error:
        levelS = "E";
        break;
    case LogLevel::Fatal:
        levelS = "F";
        break;
    default:
        break;
    }

    std::string timeS = strFromTime();

    logFile << "[" << timeS << "][" << levelS << "] " << message << "\n";
    logFile.flush();
}

int main(int argc, char* argv[])
{
    bool debugMode = false;
    std::string debugInputFilename = "";

    // Check for the argument
    if (!debugMode && (argc < 2))
    {
        std::cout << "An argument for the input filename is required.\n";
        return 1;
    }

    // Retrieve input filename
    std::string inpFilename = debugMode ? debugInputFilename : argv[1];
    if (!std::filesystem::exists(inpFilename))
    {
        std::cout << "Input file does not exist.\n";
        return 1;
    }

    // Setup log file
    std::string logFilename = inpFilename + "log.txt";
    logFile.open(logFilename, std::ofstream::out | std::ofstream::trunc);
    if (!logFile.is_open())
    {
        std::cout << "Log file could not be created/opened.\n";
        return 1;
    }
    else
    {
        std::cout << "Further information will be written into \"" << logFilename << "\".\n";
        logAdd(LogLevel::Info, strFormat("%s v%s", APP_TITLE, Config::S_APP_VERSION));
    }

    // Set Locale
    if (!std::setlocale(LC_ALL, Config::S_APP_LOCALE))
        logAdd(LogLevel::Warning, strFormat("Couldn't set locale to \"%s\".", Config::S_APP_LOCALE));

    // Log printErr
    setPrintHandler(
        [](std::string s)
        {
            logAdd(LogLevel::Info, s);
        }
    );

    // Log filenames
    std::string outFilename = inpFilename + "out";
    logAdd(LogLevel::Info, "Input filename: " + inpFilename);
    logAdd(LogLevel::Info, "Output filename: " + outFilename);

    // Create output file
    std::ofstream outFile;
    outFile.open(outFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        logAdd(LogLevel::Fatal, "Output file could not be created/opened.");
        return 1;
    }

    // Open input file
    std::ifstream inpFile;
    inpFile.open(inpFilename, std::ifstream::in | std::ifstream::binary);
    if (!inpFile.is_open() || inpFile.fail())
    {
        logAdd(LogLevel::Fatal, "Input file could not be opened.");
        return 1;
    }
    else
    {
        inpFile.seekg(std::ifstream::beg);
    }

    // Read the operation type
    GpuHelperOperationType opType = (GpuHelperOperationType)stmReadScalar<uint32_t>(inpFile);
    if (inpFile.fail())
    {
        logAdd(LogLevel::Fatal, "Failed to read the operation type.");
        return 1;
    }

    // Map operation types to functions
    std::unordered_map<GpuHelperOperationType, GpuHelperOperation> opMap;
    opMap[GpuHelperOperationType::Dispersion] = runDisp;
    opMap[GpuHelperOperationType::ConvFFT] = runConvFFT;
    opMap[GpuHelperOperationType::ConvNaive] = runConvNaive;

    // Check if the operation has a function associated
    if (opMap.count(opType) < 1)
    {
        logAdd(LogLevel::Fatal, "Operation not implemented.");
        return 1;
    }

    // Create OpenGL context
    {
        // OpenGL version

        bool useCompute =
            (opType == GpuHelperOperationType::Diffraction)
            || (opType == GpuHelperOperationType::ConvFFT);

        int glVersionMajor, glVersionMinor;
        if (useCompute)
        {
            glVersionMajor = 4;
            glVersionMinor = 5;
        }
        else
        {
            glVersionMajor = 3;
            glVersionMinor = 2;
        }

        // Context

        std::string ctxError;
        bool ctxSuccess = oglOneTimeContext(
            glVersionMajor, glVersionMinor,
            [&opType, &opMap, &inpFilename, &outFilename, &inpFile, &outFile]()
            {
                // Start the operation
                logAdd(LogLevel::Info, strFormat("Renderer: %s", (const char*)glGetString(GL_RENDERER)));
                logAdd(LogLevel::Info, strFormat("Operation type: %s", strFromGpuHelperOperationType(opType).c_str()));
                logAdd(LogLevel::Info, "Starting operation...");
                opMap[opType](inpFilename, outFilename, inpFile, outFile);
            },
            ctxError);

        if (!ctxSuccess)
        {
            logAdd(LogLevel::Fatal, strFormat("OpenGL context initialization error: %s", ctxError.c_str()));
            return 1;
        }
    }

    // Close the output file
    outFile.flush();
    outFile.close();
    if (outFile.fail())
        logAdd(LogLevel::Error, "There was a problem when writing the output.");
    else
        logAdd(LogLevel::Info, "Output was successfully written.");

    return 0;
}

void runDisp(std::string inpFilename, std::string outFilename, std::ifstream& inpFile, std::ofstream& outFile)
{
    // Read the input

    logAdd(LogLevel::Info, "Attempting to read the input data...");

    bool readInput = false;
    std::string readInputError = "";
    RealBloom::BinaryDispGpuInput binInput;
    try
    {
        binInput.readFrom(inpFile);
        readInput = true;
    }
    catch (const std::exception& e)
    {
        readInputError = e.what();
        logAdd(LogLevel::Error, e.what());
    }
    inpFile.close();

    // DispGpu
    std::shared_ptr<RealBloom::DispGpu> disp = nullptr;

    // Final status to write into the output
    bool finalSuccess = true;
    std::string finalError = "";

    if (readInput)
    {
        // Start

        logAdd(LogLevel::Info, "Starting dispersion...");

        disp = std::make_shared<RealBloom::DispGpu>(&binInput);
        disp->process();

        finalSuccess = disp->getStatus().isOK();
        finalError = disp->getStatus().getError();

        if (finalSuccess)
        {
            logAdd(LogLevel::Info, "Dispersion is done.");
        }
        else
        {
            logAdd(LogLevel::Error, "Dispersion was failed.");
            logAdd(LogLevel::Error, finalError);
        }
    }

    // Setup binary output
    RealBloom::BinaryDispGpuOutput binOutput;
    if (readInput)
    {
        if (finalSuccess)
        {
            binOutput.status = 1;
            binOutput.buffer = disp->getBuffer();
        }
        else
        {
            binOutput.status = 0;
            binOutput.error = finalError;
        }
    }
    else
    {
        binOutput.status = 0;
        binOutput.error = readInputError;
    }

    // Write the output
    logAdd(LogLevel::Info, "Writing the output...");
    try
    {
        binOutput.writeTo(outFile);
    }
    catch (const std::exception& e)
    {
        logAdd(LogLevel::Error, e.what());
    }
}

void runConvFFT(std::string inpFilename, std::string outFilename, std::ifstream& inpFile, std::ofstream& outFile)
{
    // Read the input

    logAdd(LogLevel::Info, "Attempting to read the input data...");

    bool readInput = false;
    std::string readInputError = "";
    RealBloom::BinaryConvFftGpuInput binInput;
    try
    {
        binInput.readFrom(inpFile);
        readInput = true;
    }
    catch (const std::exception& e)
    {
        readInputError = e.what();
        logAdd(LogLevel::Error, e.what());
    }
    inpFile.close();

    // ConvFftGpu
    std::shared_ptr<RealBloom::ConvFftGpu> conv = nullptr;

    // Final status to write into the output
    bool finalSuccess = true;
    std::string finalError = "";

    if (readInput)
    {
        try
        {
            // Initialize
            logAdd(LogLevel::Info, "Initializing...");
            conv = std::make_shared<RealBloom::ConvFftGpu>(&binInput);

            // Prepare
            logAdd(LogLevel::Info, "Preparing...");
            conv->pad();

            // Repeat for 3 color channels
            for (uint32_t i = 0; i < 3; i++)
            {
                // Input FFT
                logAdd(LogLevel::Info, strFormat("%s: Input FFT", strFromColorChannelID(i).c_str()));
                conv->inputFFT(i);

                // Kernel FFT
                logAdd(LogLevel::Info, strFormat("%s: Kernel FFT", strFromColorChannelID(i).c_str()));
                conv->kernelFFT(i);

                // Multiply
                logAdd(LogLevel::Info, strFormat("%s: Multiplying", strFromColorChannelID(i).c_str()));
                conv->multiply(i);

                // Multiply
                logAdd(LogLevel::Info, strFormat("%s: Inverse FFT", strFromColorChannelID(i).c_str()));
                conv->inverse(i);
            }

            // Get the final output
            logAdd(LogLevel::Info, "Finalizing...");
            conv->output();

            logAdd(LogLevel::Info, "Convolution is done.");
        }
        catch (const std::exception& e)
        {
            logAdd(LogLevel::Error, "Convolution was failed.");
            logAdd(LogLevel::Error, e.what());
            finalSuccess = false;
            finalError = e.what();
        }
    }

    // Setup binary output
    RealBloom::BinaryConvFftGpuOutput binOutput;
    if (readInput)
    {
        if (finalSuccess)
        {
            binOutput.status = 1;
            binOutput.buffer = conv->getBuffer();
        }
        else
        {
            binOutput.status = 0;
            binOutput.error = finalError;
        }
    }
    else
    {
        binOutput.status = 0;
        binOutput.error = readInputError;
    }

    // Write the output
    logAdd(LogLevel::Info, "Writing the output...");
    try
    {
        binOutput.writeTo(outFile);
    }
    catch (const std::exception& e)
    {
        logAdd(LogLevel::Error, e.what());
    }
}

void runConvNaive(std::string inpFilename, std::string outFilename, std::ifstream& inpFile, std::ofstream& outFile)
{
    // Read the input

    logAdd(LogLevel::Info, "Attempting to read the input data...");

    bool readInput = false;
    std::string readInputError = "";
    RealBloom::BinaryConvNaiveGpuInput binInput;
    try
    {
        binInput.readFrom(inpFile);
        readInput = true;
    }
    catch (const std::exception& e)
    {
        readInputError = e.what();
        logAdd(LogLevel::Error, e.what());
    }
    inpFile.close();

    // Final status to write into the output
    bool finalSuccess = true;
    std::string finalError = "";

    // Allocate the final buffer
    uint32_t inputSize = binInput.inputWidth * binInput.inputHeight * 4;
    std::vector<float> finalBuffer;
    finalBuffer.resize(inputSize);

    // Fill with black
    for (uint32_t i = 0; i < inputSize; i++)
        finalBuffer[i] = (i % 4 == 3) ? 1.0f : 0.0f;

    // Stat file info
    std::string statFilename = inpFilename + "stat";
    std::string statMutexName = binInput.statMutexName;

    // How frequently to write the stat
    std::chrono::time_point<std::chrono::system_clock> lastStatWriteTime = std::chrono::system_clock::now();
    uint32_t statWriteInterval = 1; // write stat after every chunk
    if (binInput.numChunks > 15) statWriteInterval = 2; // every 2 chunks
    if (binInput.numChunks > 30) statWriteInterval = 3; // every 3 chunks
    if (binInput.numChunks > 50) statWriteInterval = 4; // every 4 chunks

    if (readInput)
    {
        // Prepare

        logAdd(LogLevel::Info, "Preparing convolution...");

        RealBloom::ConvNaiveGpu conv(&binInput);
        conv.prepare();

        if (!conv.getStatus().isOK())
        {
            conv.cleanUp();

            finalSuccess = false;
            finalError = conv.getStatus().getError();
            clearVector(finalBuffer);

            logAdd(LogLevel::Error, "Failed to prepare convolution.");
            logAdd(LogLevel::Error, finalError);
        }
        else
        {
            // Start convolution
            logAdd(LogLevel::Info, "Starting convolution...");
            for (uint32_t i = 0; i < binInput.numChunks; i++)
            {
                conv.process(i);

                if (conv.getStatus().isOK())
                {
                    for (uint32_t j = 0; j < inputSize; j++)
                        if (j % 4 != 3) // if not alpha channel
                            finalBuffer[j] += conv.getBuffer()[j];

                    logAdd(LogLevel::Info, strFormat(
                        "Chunk %u/%u (%u points) was successful.", i + 1, binInput.numChunks, conv.getNumVertices()));
                }
                else
                {
                    finalSuccess = false;
                    finalError = conv.getStatus().getError();
                    clearVector(finalBuffer);

                    logAdd(LogLevel::Error, strFormat(
                        "Chunk %u/%u (%u points) was failed.", i + 1, binInput.numChunks, conv.getNumVertices()));
                    logAdd(LogLevel::Error, finalError);
                    break;
                }

                // Write stat file to report the progress
                if ((binInput.numChunks > 1)
                    && finalSuccess
                    && (getElapsedMs(lastStatWriteTime) > 500)
                    && (i % statWriteInterval == 0)
                    && (i < (binInput.numChunks - 1))) // not the last chunk
                {
                    RealBloom::BinaryConvNaiveGpuStat binStat;
                    binStat.numChunksDone = i + 1;
                    binStat.buffer = finalBuffer;

                    HANDLE hMutex = openMutex(statMutexName);
                    if (hMutex == NULL)
                        logAdd(LogLevel::Warning, strFormat("Mutex \"%s\" could not be opened.", statMutexName.c_str()));

                    waitForMutex(hMutex);

                    std::ofstream statFile;
                    statFile.open(statFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
                    if (!statFile.is_open())
                    {
                        logAdd(LogLevel::Error, strFormat("Stat file \"%s\" could not be created/opened.", statFilename.c_str()));
                    }
                    else
                    {
                        try
                        {
                            binStat.writeTo(statFile);
                            logAdd(LogLevel::Info, "Stat was successfully written.");
                        }
                        catch (const std::exception& e)
                        {
                            logAdd(LogLevel::Error, strFormat("Failed to write the stat: %s", e.what()));
                        }
                        statFile.flush();
                        statFile.close();
                    }

                    releaseMutex(hMutex);
                    closeMutex(hMutex);
                    lastStatWriteTime = std::chrono::system_clock::now();
                }

                // Sleep
                if ((binInput.chunkSleep > 0) && (i < (binInput.numChunks - 1)))
                {
                    logAdd(LogLevel::Info, strFormat("Sleeping for %u ms...", binInput.chunkSleep));

                    auto t1 = std::chrono::system_clock::now();
                    while (getElapsedMs(t1) < binInput.chunkSleep)
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            // Clean up
            conv.cleanUp();
            logAdd(LogLevel::Info, "Convolution is done.");
        }
    }

    // Setup binary output
    RealBloom::BinaryConvNaiveGpuOutput binOutput;
    if (readInput)
    {
        if (finalSuccess)
        {
            binOutput.status = 1;
            binOutput.buffer = finalBuffer;
        }
        else
        {
            binOutput.status = 0;
            binOutput.error = finalError;
        }
    }
    else
    {
        binOutput.status = 0;
        binOutput.error = readInputError;
    }

    // Write the output
    logAdd(LogLevel::Info, "Writing the output...");
    try
    {
        binOutput.writeTo(outFile);
    }
    catch (const std::exception& e)
    {
        logAdd(LogLevel::Error, e.what());
    }
}
