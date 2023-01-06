#pragma once

#include <string>
#include <vector>
#include <stdint.h>

#include "BinaryData.h"

namespace RealBloom
{

    class BinaryDispGpuInput : public BinaryData
    {
    protected:
        std::string getType() override;
        void readInternal(std::istream& stream) override;
        void writeInternal(std::ostream& stream) override;

    public:
        BinaryDispGpuInput() {};
        ~BinaryDispGpuInput() {};

        float dp_amount = 0.0f;
        uint32_t dp_steps = 32;

        uint32_t inputWidth = 0;
        uint32_t inputHeight = 0;

        std::vector<float> inputBuffer;
        std::vector<float> cmfSamples;
    };

    class BinaryDispGpuOutput : public BinaryData
    {
    protected:
        std::string getType() override;
        void readInternal(std::istream& stream) override;
        void writeInternal(std::ostream& stream) override;

    public:
        BinaryDispGpuOutput() {};
        ~BinaryDispGpuOutput() {};

        uint32_t status = 0; // 0: failure, 1: success
        std::string error = "";
        std::vector<float> buffer;
    };

}
