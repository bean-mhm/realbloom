#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>

#include "Utils/CliParser.h"
#include "Utils/Misc.h"

class CLI
{
private:
    struct CliVars
    {
        std::shared_ptr<CliParser> parser = nullptr;
        bool hasCommands = false;
    };
    static CliVars* S_VARS;

public:
    CLI() = delete;
    CLI(const CLI&) = delete;
    CLI& operator= (const CLI&) = delete;

    static bool init(int& argc, char** argv);
    static void cleanUp();

    static bool hasCommands();
    static void proceed();
};
