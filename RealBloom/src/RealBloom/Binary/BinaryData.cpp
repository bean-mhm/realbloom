#include "BinaryData.h"

namespace RealBloom
{

    std::string BinaryData::makeErrorSource(const char* baseFunctionName)
    {
        return strFormat("%s (%s)", baseFunctionName, getType().c_str());
    }

    std::string BinaryData::getDefaultHeader()
    {
        return strFormat("RealBloom Binary Data v%s; Type: %s;", Config::S_APP_VERSION, getType().c_str());
    }

    void BinaryData::readFrom(std::istream& stream)
    {
        stmCheck(stream, makeErrorSource(__FUNCTION__), "init");

        // Read the header
        std::string header = stmReadString(stream);
        stmCheck(stream, makeErrorSource(__FUNCTION__), "header");

        // Verify the header
        std::string defaultHeader = getDefaultHeader();
        if (header != defaultHeader)
            throw std::exception(makeError(makeErrorSource(__FUNCTION__), "", strFormat(
                "Header mismatch. Expected: \"%s\", Read: \"%s\"", defaultHeader.c_str(), header.c_str()
            )).c_str());

        // Call the derived function to read the body
        readInternal(stream);
    }

    void BinaryData::writeTo(std::ostream& stream)
    {
        stmCheck(stream, makeErrorSource(__FUNCTION__), "init");

        // Write the header
        std::string header = getDefaultHeader();
        stmWriteString(stream, header);
        stmCheck(stream, makeErrorSource(__FUNCTION__), "header");

        // Call the derived function to write the body
        writeInternal(stream);
    }

}
