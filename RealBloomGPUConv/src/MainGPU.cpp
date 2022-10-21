#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <chrono>

#include "RealBloom/ConvolutionGPU.h"
#include "RealBloom/ConvolutionGPUBinary.h"
#include "Utils/Misc.h"

constexpr const char* APP_TITLE = "RealBloom GPU Convolution Helper";
constexpr const char* APP_VERSION = "0.2.0-beta";

// Use the dedicated GPU on Dual-GPU systems
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

std::ofstream logFile;

void parseTime(std::string& outTimeS)
{
    time_t rawtime;
    struct tm timeinfo;
    char buffer[128];

    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

    outTimeS = buffer;
}

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

    std::string timeS;
    parseTime(timeS);

    logFile << "[" << timeS << "][" << levelS << "] " << message << "\n";
    logFile.flush();
}

void logFail(const std::string& message)
{
    logAdd(LogLevel::Fatal, message);
    logFile.close();
}

int main(int argc, char* argv[])
{
    // Check for the argument
    if (argc < 2)
    {
        std::cout << "An argument for the input filename is required.\n";
        return 1;
    }

    // Retrieve input filename
    std::string inpFilename = argv[1];
    if (!std::filesystem::exists(inpFilename))
    {
        std::cout << "Input file does not exist!\n";
        return 1;
    }

    // Setup log file
    std::string logFilename = inpFilename + "log.txt";
    logFile.open(logFilename, std::ofstream::out | std::ofstream::trunc);
    if (!logFile.is_open())
    {
        std::cout << "Log file could not be created/opened.\n";
        return 1;
    } else
    {
        std::cout << "Further information will be written into \"" << logFilename << "\".\n";
        logAdd(LogLevel::Info, stringFormat("%s v%s", APP_TITLE, APP_VERSION));
    }

    // Log filenames
    std::string outFilename = inpFilename + "out";
    logAdd(LogLevel::Info, "Input filename: " + inpFilename);
    logAdd(LogLevel::Info, "Output filename: " + outFilename);

    // Create output file
    std::ofstream outFile;
    outFile.open(outFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        logFail("Output file could not be created/opened.");
        return 1;
    }

    // Open input file
    std::ifstream inpFile;
    inpFile.open(inpFilename, std::ifstream::in | std::ifstream::binary);
    if (!inpFile.is_open() || inpFile.fail())
    {
        logFail("Input file could not be opened.");
        return 1;
    } else
    {
        inpFile.seekg(std::ifstream::beg);
    }

    // Parse input
    RealBloom::ConvolutionGPUBinaryInput cgInput;
    logAdd(LogLevel::Info, "Attempting to read the input data...");

    bool parseFailed = false;
    std::string parseStage = "Initialization";
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.statMutexNameSize), sizeof(cgInput.statMutexNameSize));
        parseFailed |= inpFile.fail();
        parseStage = "statMutexNameSize";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.numChunks), sizeof(cgInput.numChunks));
        parseFailed |= inpFile.fail();
        parseStage = "numChunks";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.chunkSleep), sizeof(cgInput.chunkSleep));
        parseFailed |= inpFile.fail();
        parseStage = "chunkSleep";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.cp_kernelCenterX), sizeof(cgInput.cp_kernelCenterX));
        parseFailed |= inpFile.fail();
        parseStage = "cp_kernelCenterX";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.cp_kernelCenterY), sizeof(cgInput.cp_kernelCenterY));
        parseFailed |= inpFile.fail();
        parseStage = "cp_kernelCenterY";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.cp_convThreshold), sizeof(cgInput.cp_convThreshold));
        parseFailed |= inpFile.fail();
        parseStage = "cp_convThreshold";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.cp_convKnee), sizeof(cgInput.cp_convKnee));
        parseFailed |= inpFile.fail();
        parseStage = "cp_convKnee";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.convMultiplier), sizeof(cgInput.convMultiplier));
        parseFailed |= inpFile.fail();
        parseStage = "convMultiplier";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.inputWidth), sizeof(cgInput.inputWidth));
        parseFailed |= inpFile.fail();
        parseStage = "inputWidth";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.inputHeight), sizeof(cgInput.inputHeight));
        parseFailed |= inpFile.fail();
        parseStage = "inputHeight";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.kernelWidth), sizeof(cgInput.kernelWidth));
        parseFailed |= inpFile.fail();
        parseStage = "kernelWidth";
    }
    if (!parseFailed)
    {
        inpFile.read(AS_BYTES(cgInput.kernelHeight), sizeof(cgInput.kernelHeight));
        parseFailed |= inpFile.fail();
        parseStage = "kernelHeight";
    }
    if (!parseFailed)
    {
        cgInput.statMutexNameBuffer = new char[cgInput.statMutexNameSize + 1];
        cgInput.statMutexNameBuffer[cgInput.statMutexNameSize] = '\0';

        inpFile.read(cgInput.statMutexNameBuffer, cgInput.statMutexNameSize);
        parseFailed |= inpFile.fail();
        parseStage = "statMutexNameBuffer";
    }
    if (!parseFailed)
    {
        uint32_t inputSize = cgInput.inputWidth * cgInput.inputHeight * 4;
        if (inputSize > 0)
        {
            cgInput.inputBuffer = new float[inputSize];
            inpFile.read(PTR_AS_BYTES(cgInput.inputBuffer), inputSize * sizeof(float));
            parseFailed |= inpFile.fail();
            parseStage = "inputBuffer";
        }
    }
    if (!parseFailed)
    {
        uint32_t kernelSize = cgInput.kernelWidth * cgInput.kernelHeight * 4;
        if (kernelSize > 0)
        {
            cgInput.kernelBuffer = new float[kernelSize];
            inpFile.read(PTR_AS_BYTES(cgInput.kernelBuffer), kernelSize * sizeof(float));
            parseFailed |= inpFile.fail();
            parseStage = "kernelBuffer";
        }
    }
    inpFile.close();
    logAdd(LogLevel::Info, "Closed the input file.");

    logAdd(LogLevel::Debug, "Chunk sleep: " + std::to_string(cgInput.chunkSleep));

    // Will be manipulated later
    bool cgFinalSuccess = true;
    std::string cgFinalError = "";
    std::vector<float> cgFinalBuffer;

    // Prepare the final buffer
    uint32_t inputSize = cgInput.inputWidth * cgInput.inputHeight * 4;
    cgFinalBuffer.resize(inputSize);
    for (uint32_t i = 0; i < inputSize; i++)
    {
        if (i % 4 == 3) // alpha channel
            cgFinalBuffer[i] = 1.0f;
        else
            cgFinalBuffer[i] = 0.0f;
    }

    // Start convolution if input was read successfully
    if (!parseFailed)
    {
        logAdd(LogLevel::Info, "Starting GPU Convolution...");

        bool hasRenderer = false;
        std::string renderer = "Unknown";

        std::string statFilename = inpFilename + "stat";
        std::string statMutexName = cgInput.statMutexNameBuffer;

        std::chrono::time_point<std::chrono::system_clock> lastStatWriteTime = std::chrono::system_clock::now();
        uint32_t statWriteInterval = 1; // write stat every x chunks
        if (cgInput.numChunks > 15) statWriteInterval = 2; // every 2 chunks
        if (cgInput.numChunks > 30) statWriteInterval = 3; // every 3 chunks
        if (cgInput.numChunks > 50) statWriteInterval = 4; // every 4 chunks

        for (uint32_t i = 0; i < cgInput.numChunks; i++)
        {
            RealBloom::ConvolutionGPUData cgData;
            cgData.binaryInput = &cgInput;

            RealBloom::ConvolutionGPU conv(&cgData);
            conv.start(cgInput.numChunks, i);

            if (cgData.success)
            {
                for (uint32_t j = 0; j < inputSize; j++)
                    if (j % 4 != 3) // if not alpha channel
                        cgFinalBuffer[j] += cgData.outputBuffer[j];
            } else
            {
                cgFinalSuccess = false;
                cgFinalError = cgData.error;
                cgFinalBuffer.clear();
            }

            if (!hasRenderer)
            {
                renderer = cgData.gpuName;
                hasRenderer = true;
            }

            if (cgFinalSuccess)
            {
                logAdd(LogLevel::Info, stringFormat(
                    "Chunk %d/%d (%d points) was successful.", i + 1, cgInput.numChunks, cgData.numPoints));
            } else
            {
                logAdd(LogLevel::Error, stringFormat(
                    "Chunk %d/%d (%d points) was failed.", i + 1, cgInput.numChunks, cgData.numPoints));
                break;
            }

            // Write stat file to show the progress
            if ((cgInput.numChunks > 1)
                && cgFinalSuccess
                && (getElapsedMs(lastStatWriteTime) > 500)
                && (i % statWriteInterval == 0)
                && (i < (cgInput.numChunks - 1))) // not the last chunk
            {
                RealBloom::ConvolutionGPUBinaryStat cgStat;
                cgStat.numChunksDone = i + 1;
                cgStat.bufferSize = cgFinalBuffer.size();
                cgStat.buffer = cgFinalBuffer.data();

                HANDLE hMutex = openMutex(statMutexName);
                if (hMutex == NULL)
                    logAdd(LogLevel::Warning, stringFormat("Mutex \"%s\" could not be opened.", statMutexName.c_str()));

                waitForMutex(hMutex);

                std::ofstream statFile;
                statFile.open(statFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
                if (!statFile.is_open())
                {
                    logAdd(LogLevel::Error, stringFormat("Stat file \"%s\" could not be created/opened.", statFilename.c_str()));
                } else
                {
                    {
                        // numChunksDone
                        statFile.write(AS_BYTES(cgStat.numChunksDone), sizeof(cgStat.numChunksDone));

                        // bufferSize
                        statFile.write(AS_BYTES(cgStat.bufferSize), sizeof(cgStat.bufferSize));

                        // buffer
                        if ((cgStat.bufferSize > 0) && (cgStat.buffer != nullptr))
                            statFile.write(PTR_AS_BYTES(cgStat.buffer), cgStat.bufferSize * sizeof(float));
                    }
                    statFile.flush();
                    statFile.close();
                    if (statFile.fail())
                        logAdd(LogLevel::Error, "There was a problem when writing the stat.");
                    else
                        logAdd(LogLevel::Info, "Stat was successfully written.");
                }

                releaseMutex(hMutex);
                closeMutex(hMutex);
                lastStatWriteTime = std::chrono::system_clock::now();
            }
        }

        logAdd(LogLevel::Info, stringFormat(
            "GPU Convolution is done. Renderer: \"%s\"",
            renderer.c_str())
        );
    }

    // Clean up the input buffers
    DELARR(cgInput.statMutexNameBuffer);
    DELARR(cgInput.inputBuffer);
    DELARR(cgInput.kernelBuffer);

    // Setup binary output
    RealBloom::ConvolutionGPUBinaryOutput cgOutput;
    std::string cgOutputError = "";
    if (!parseFailed)
    {
        if (cgFinalSuccess)
        {
            cgOutput.status = 1;
            cgOutput.bufferSize = cgFinalBuffer.size();
            cgOutput.buffer = cgFinalBuffer.data();
        } else
        {
            cgOutputError = cgFinalError;
            logAdd(LogLevel::Error, "GPU Convolution: " + cgOutputError);

            cgOutput.status = 0;
            cgOutput.bufferSize = 0;
            cgOutput.buffer = nullptr;
        }
    } else
    {
        cgOutputError = "Could not retrieve data from the input file, failed at \"" + parseStage;
        cgOutputError += "\".";
        logAdd(LogLevel::Error, cgOutputError);

        cgOutput.status = 0;
        cgOutput.bufferSize = 0;
        cgOutput.buffer = nullptr;
    }

    // Write the output (status, errorSize, bufferSize, errorBuffer, buffer)
    logAdd(LogLevel::Info, "Writing the output...");
    {
        // status
        outFile.write(AS_BYTES(cgOutput.status), sizeof(cgOutput.status));

        cgOutput.errorSize = cgOutputError.empty() ? 0 : cgOutputError.size();
        cgOutput.errorBuffer = nullptr;
        if (cgOutput.errorSize > 0)
        {
            cgOutput.errorBuffer = new char[cgOutput.errorSize];
            cgOutputError.copy(cgOutput.errorBuffer, cgOutput.errorSize);
        }

        // errorSize
        outFile.write(AS_BYTES(cgOutput.errorSize), sizeof(cgOutput.errorSize));

        // bufferSize
        outFile.write(AS_BYTES(cgOutput.bufferSize), sizeof(cgOutput.bufferSize));

        // errorBuffer
        if (cgOutput.errorSize > 0)
        {
            outFile.write(cgOutput.errorBuffer, cgOutput.errorSize);
            DELARR(cgOutput.errorBuffer);
        }

        // buffer
        if ((cgOutput.bufferSize > 0) && (cgOutput.buffer != nullptr))
        {
            outFile.write(PTR_AS_BYTES(cgOutput.buffer), cgOutput.bufferSize * sizeof(float));
        }
    }
    outFile.flush();
    outFile.close();
    if (outFile.fail())
        logAdd(LogLevel::Error, "There was a problem when writing the output.");
    else
        logAdd(LogLevel::Info, "Output was successfully written.");

    cgFinalBuffer.clear();
    logFile.close();
    return 0;
}