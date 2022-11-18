#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

#include "Utils/CliParser.h"

struct CliArgument
{
    std::vector<std::string> aliases;
    std::string desc;
    bool required;
};

struct CliCommand
{
    std::string name;
    std::string desc;
    std::string example;
    std::vector<CliArgument> arguments;
    std::function<void(const CliCommand&, const CliParser&, const std::unordered_map<std::string, std::string>&)> action;
};

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

    static void init(int& argc, char** argv);
    static void cleanUp();

    static bool hasCommands();
    static void proceed();
};
