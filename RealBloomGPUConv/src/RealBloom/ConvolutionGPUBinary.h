#pragma once

#include <stdint.h>
#include <string>

namespace RealBloom
{

    struct ConvolutionGPUBinaryInput
    {
        uint32_t statMutexNameSize = 0;
        uint32_t numChunks = 1;

        float cp_kernelCenterX = 0.5f;
        float cp_kernelCenterY = 0.5f;
        float cp_convThreshold = 0.5f;
        float cp_convKnee = 0.2f;
        float convMultiplier = 0.001f;

        uint32_t inputWidth = 0;
        uint32_t inputHeight = 0;

        uint32_t kernelWidth = 0;
        uint32_t kernelHeight = 0;

        char* statMutexNameBuffer = nullptr;
        float* inputBuffer = nullptr;
        float* kernelBuffer = nullptr;
    };

    struct ConvolutionGPUBinaryOutput
    {
        uint32_t status = 0; // 0 for failure and 1 for success
        uint32_t errorSize = 0;
        uint32_t bufferSize = 0; // number of float elements in the output buffer
        char* errorBuffer = nullptr;
        float* buffer = nullptr;
    };

    struct ConvolutionGPUBinaryStat
    {
        uint32_t numChunksDone = 0;
        uint32_t bufferSize = 0;
        float* buffer = nullptr;
    };

}