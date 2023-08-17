#include "BinaryDispGpu.h"

namespace RealBloom
{

    std::string BinaryDispGpuInput::getType()
    {
        return "BinaryDispGpuInput";
    }

    void BinaryDispGpuInput::readInternal(std::istream& stream)
    {
        dp_amount = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "dp_amount");

        dp_edgeOffset = stmReadScalar<float>(stream);
        stmCheck(stream, __FUNCTION__, "dp_edgeOffset");

        dp_steps = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "dp_steps");

        inputWidth = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "inputWidth");

        inputHeight = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "inputHeight");

        stmReadVector(stream, inputBuffer);
        stmCheck(stream, __FUNCTION__, "inputBuffer");

        stmReadVector(stream, cmfSamples);
        stmCheck(stream, __FUNCTION__, "cmfSamples");
    }

    void BinaryDispGpuInput::writeInternal(std::ostream& stream)
    {
        stmWriteScalar(stream, dp_amount);
        stmCheck(stream, __FUNCTION__, "dp_amount");

        stmWriteScalar(stream, dp_edgeOffset);
        stmCheck(stream, __FUNCTION__, "dp_edgeOffset");

        stmWriteScalar(stream, dp_steps);
        stmCheck(stream, __FUNCTION__, "dp_steps");

        stmWriteScalar(stream, inputWidth);
        stmCheck(stream, __FUNCTION__, "inputWidth");

        stmWriteScalar(stream, inputHeight);
        stmCheck(stream, __FUNCTION__, "inputHeight");

        stmWriteVector(stream, inputBuffer);
        stmCheck(stream, __FUNCTION__, "inputBuffer");

        stmWriteVector(stream, cmfSamples);
        stmCheck(stream, __FUNCTION__, "cmfSamples");
    }

    std::string BinaryDispGpuOutput::getType()
    {
        return "BinaryDispGpuOutput";
    }

    void BinaryDispGpuOutput::readInternal(std::istream& stream)
    {
        status = stmReadScalar<uint32_t>(stream);
        stmCheck(stream, __FUNCTION__, "status");

        error = stmReadString(stream);
        stmCheck(stream, __FUNCTION__, "error");

        stmReadVector(stream, buffer);
        stmCheck(stream, __FUNCTION__, "buffer");
    }

    void BinaryDispGpuOutput::writeInternal(std::ostream& stream)
    {
        stmWriteScalar(stream, status);
        stmCheck(stream, __FUNCTION__, "status");

        stmWriteString(stream, error);
        stmCheck(stream, __FUNCTION__, "error");

        stmWriteVector(stream, buffer);
        stmCheck(stream, __FUNCTION__, "buffer");
    }

}
