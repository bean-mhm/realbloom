#include "BinaryConvNaiveGpu.h"

namespace RealBloom
{

    std::string BinaryConvNaiveGpuInput::getType()
    {
        return "BinaryConvNaiveGpuInput";
    }

    void BinaryConvNaiveGpuInput::readInternal(std::istream& stream)
    {
        statMutexName = stmReadString(stream);
        stmCheck(stream, __FUNCTION__, "statMutexName");

        numChunks = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "numChunks");

        chunkSleep = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "chunkSleep");

        cp_kernelCenterX = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "cp_kernelCenterX");

        cp_kernelCenterY = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "cp_kernelCenterY");

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

    void BinaryConvNaiveGpuInput::writeInternal(std::ostream& stream)
    {
        stmWriteString(stream, statMutexName);
        stmCheck(stream, __FUNCTION__, "statMutexName");

        stmWriteScalar(stream, numChunks);
        stmCheck(stream, __FUNCTION__, "numChunks");

        stmWriteScalar(stream, chunkSleep);
        stmCheck(stream, __FUNCTION__, "chunkSleep");

        stmWriteScalar(stream, cp_kernelCenterX);
        stmCheck(stream, __FUNCTION__, "cp_kernelCenterX");

        stmWriteScalar(stream, cp_kernelCenterY);
        stmCheck(stream, __FUNCTION__, "cp_kernelCenterY");

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

    std::string BinaryConvNaiveGpuOutput::getType()
    {
        return "BinaryConvNaiveGpuOutput";
    }

    void BinaryConvNaiveGpuOutput::readInternal(std::istream& stream)
    {
        status = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "status");

        error = stmReadString(stream);
        stmCheck(stream, __FUNCTION__, "error");

        stmReadVector(stream, buffer);
        stmCheck(stream, __FUNCTION__, "buffer");
    }

    void BinaryConvNaiveGpuOutput::writeInternal(std::ostream& stream)
    {
        stmWriteScalar(stream, status);
        stmCheck(stream, __FUNCTION__, "status");

        stmWriteString(stream, error);
        stmCheck(stream, __FUNCTION__, "error");

        stmWriteVector(stream, buffer);
        stmCheck(stream, __FUNCTION__, "buffer");
    }

    std::string BinaryConvNaiveGpuStat::getType()
    {
        return "BinaryConvNaiveGpuStat";
    }

    void BinaryConvNaiveGpuStat::readInternal(std::istream& stream)
    {
        numChunksDone = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "numChunksDone");

        stmReadVector(stream, buffer);
        stmCheck(stream, __FUNCTION__, "buffer");
    }

    void BinaryConvNaiveGpuStat::writeInternal(std::ostream& stream)
    {
        stmWriteScalar(stream, numChunksDone);
        stmCheck(stream, __FUNCTION__, "numChunksDone");

        stmWriteVector(stream, buffer);
        stmCheck(stream, __FUNCTION__, "buffer");
    }

}
