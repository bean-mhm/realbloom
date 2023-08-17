#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "BinaryData.h"

namespace RealBloom
{

    class BinaryConvFftGpuInput : public BinaryData
    {
    protected:
        std::string getType() override;
        void readInternal(std::istream& stream) override;
        void writeInternal(std::ostream& stream) override;

    public:
        BinaryConvFftGpuInput() {};
        ~BinaryConvFftGpuInput() {};

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

    class BinaryConvFftGpuOutput : public BinaryData
    {
    protected:
        std::string getType() override;
        void readInternal(std::istream& stream) override;
        void writeInternal(std::ostream& stream) override;

    public:
        BinaryConvFftGpuOutput() {};
        ~BinaryConvFftGpuOutput() {};

        uint32_t status = 0; // 0: failure, 1: success
        std::string error = "";
        std::vector<float> buffer;
    };

}
