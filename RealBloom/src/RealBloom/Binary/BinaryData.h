#pragma once

#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>

#include "../../Utils/Misc.h"
#include "../../Config.h"

namespace RealBloom
{

    class BinaryData
    {
    private:
        std::string makeErrorSource(const char* baseFunctionName);

    protected:
        virtual std::string getType() = 0;
        std::string getDefaultHeader();

        virtual void readInternal(std::istream& stream) = 0;
        virtual void writeInternal(std::ostream& stream) = 0;

    public:
        BinaryData() {};
        virtual ~BinaryData() {};

        void readFrom(std::istream& stream);
        void writeTo(std::ostream& stream);
    };

}
