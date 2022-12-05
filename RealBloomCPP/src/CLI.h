#pragma once

#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <thread>
#include <chrono>

#include "Utils/CliParser.h"

typedef std::unordered_map<std::string, std::string> StringMap;

struct CliArgument
{
    std::vector<std::string> aliases;
    std::string desc;
    std::string defaultValue;
    bool required;
};

struct CliCommand
{
    std::string name;
    std::string desc;
    std::string example;
    std::vector<CliArgument> arguments;
    std::function<void(const CliCommand&, const CliParser&, StringMap&, bool)> action;
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
