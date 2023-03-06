#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "BinaryData.h"

namespace RealBloom
{

    class BinaryConvNaiveGpuInput : public BinaryData
    {
    protected:
        std::string getType() override;
        void readInternal(std::istream& stream) override;
        void writeInternal(std::ostream& stream) override;

    public:
        BinaryConvNaiveGpuInput() {};
        ~BinaryConvNaiveGpuInput() {};

        std::string statMutexName = "";
        uint32_t numChunks = 1;
        uint32_t chunkSleep = 0;

        float cp_kernelOriginX = 0.5f;
        float cp_kernelOriginY = 0.5f;
        float cp_convThreshold = 0.0f;
        float cp_convKnee = 0.0f;
        float convMultiplier = 1.0f;

        uint32_t inputWidth = 0;
        uint32_t inputHeight = 0;

        uint32_t kernelWidth = 0;
        uint32_t kernelHeight = 0;

        std::vector<float> inputBuffer;
        std::vector<float> kernelBuffer;
    };

    class BinaryConvNaiveGpuOutput : public BinaryData
    {
    protected:
        std::string getType() override;
        void readInternal(std::istream& stream) override;
        void writeInternal(std::ostream& stream) override;

    public:
        BinaryConvNaiveGpuOutput() {};
        ~BinaryConvNaiveGpuOutput() {};

        uint32_t status = 0; // 0: failure, 1: success
        std::string error = "";
        std::vector<float> buffer;
    };

    class BinaryConvNaiveGpuStat : public BinaryData
    {
    protected:
        std::string getType() override;
        void readInternal(std::istream& stream) override;
        void writeInternal(std::ostream& stream) override;

    public:
        BinaryConvNaiveGpuStat() {};
        ~BinaryConvNaiveGpuStat() {};

        uint32_t numChunksDone = 0;
        std::vector<float> buffer;
    };

}
