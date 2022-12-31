#include "StreamUtils.h"

void stmCheck(std::ios_base& stream, const std::string& source, const std::string& stage)
{
    if (stream.fail())
        throw std::exception(makeError(source, stage, "Stream failure").c_str());
}

std::string stmReadString(std::istream& stream)
{
    if (stream.fail())
        return "";

    // Read the string length
    uint32_t length = 0;
    stream.read(AS_BYTES(length), sizeof(length));

    if (stream.fail())
        return "";

    if (length < 1)
        return "";

    // Create buffer
    char* buf = new char[length + 1];

    // Fill with zero
    for (uint32_t i = 0; i <= length; i++)
        buf[i] = '\0';

    // Read the content
    stream.read(buf, length);

    // Store in a string
    std::string result = buf;

    // Clean up
    delete[] buf;

    // Return
    return result;
}

void stmWriteString(std::ostream& stream, const std::string& s)
{
    if (stream.fail())
        return;

    // Write the length
    uint32_t length = s.size();
    stream.write(AS_BYTES(length), sizeof(length));

    // Write the content
    if (length > 0)
        stream.write(s.c_str(), length);
}
