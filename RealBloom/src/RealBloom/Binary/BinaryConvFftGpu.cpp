#include "BinaryConvFftGpu.h"

namespace RealBloom
{

    std::string BinaryConvFftGpuInput::getType()
    {
        return "BinaryConvFftGpuInput";
    }

    void BinaryConvFftGpuInput::readInternal(std::istream& stream)
    {
        cp_kernelOriginX = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "cp_kernelOriginX");

        cp_kernelOriginY = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "cp_kernelOriginY");

        cp_convThreshold = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "cp_convThreshold");

        cp_convKnee = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "cp_convKnee");

        convMultiplier = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "convMultiplier");

        inputWidth = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "inputWidth");

        inputHeight = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "inputHeight");

        kernelWidth = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "kernelWidth");

        kernelHeight = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "kernelHeight");

        stmReadVector(stream, inputBuffer);
        stmCheck(stream, __FUNCTION__, "inputBuffer");

        stmReadVector(stream, kernelBuffer);
        stmCheck(stream, __FUNCTION__, "kernelBuffer");
    }

    void BinaryConvFftGpuInput::writeInternal(std::ostream& stream)
    {
        stmWriteScalar(stream, cp_kernelOriginX);
        stmCheck(stream, __FUNCTION__, "cp_kernelOriginX");

        stmWriteScalar(stream, cp_kernelOriginY);
        stmCheck(stream, __FUNCTION__, "cp_kernelOriginY");

        stmWriteScalar(stream, cp_convThreshold);
        stmCheck(stream, __FUNCTION__, "cp_convThreshold");

        stmWriteScalar(stream, cp_convKnee);
        stmCheck(stream, __FUNCTION__, "cp_convKnee");

        stmWriteScalar(stream, convMultiplier);
        stmCheck(stream, __FUNCTION__, "convMultiplier");

        stmWriteScalar(stream, inputWidth);
        stmCheck(stream, __FUNCTION__, "inputWidth");

        stmWriteScalar(stream, inputHeight);
        stmCheck(stream, __FUNCTION__, "inputHeight");

        stmWriteScalar(stream, kernelWidth);
        stmCheck(stream, __FUNCTION__, "kernelWidth");

        stmWriteScalar(stream, kernelHeight);
        stmCheck(stream, __FUNCTION__, "kernelHeight");

        stmWriteVector(stream, inputBuffer);
        stmCheck(stream, __FUNCTION__, "inputBuffer");

        stmWriteVector(stream, kernelBuffer);
        stmCheck(stream, __FUNCTION__, "kernelBuffer");
    }

    std::string BinaryConvFftGpuOutput::getType()
    {
        return "BinaryConvFftGpuOutput";
    }

    void BinaryConvFftGpuOutput::readInternal(std::istream& stream)
    {
        status = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "status");

        error = stmReadString(stream);
        stmCheck(stream, __FUNCTION__, "error");

        stmReadVector(stream, buffer);
        stmCheck(stream, __FUNCTION__, "buffer");
    }

    void BinaryConvFftGpuOutput::writeInternal(std::ostream& stream)
    {
        stmWriteScalar(stream, status);
        stmCheck(stream, __FUNCTION__, "status");

        stmWriteString(stream, error);
        stmCheck(stream, __FUNCTION__, "error");

        stmWriteVector(stream, buffer);
        stmCheck(stream, __FUNCTION__, "buffer");
    }

}
