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

namespace CLI
{

    typedef std::unordered_map<std::string, std::string> StringMap;

    enum class ArgumentType
    {
        Optional,
        Conditional,
        Required
    };

    struct Argument
    {
        std::vector<std::string> aliases;
        std::string desc;
        std::string defaultValue;
        ArgumentType type;
    };

    struct Command
    {
        std::string name;
        std::string desc;
        std::string example;
        std::vector<Argument> arguments;
        std::function<void(const Command&, const CliParser&, StringMap&, bool)> action;
        bool hasVerbose = false;
    };

    class Interface
    {
    private:
        static bool S_ACTIVE;

    public:
        Interface() = delete;
        Interface(const Interface&) = delete;
        Interface& operator= (const Interface&) = delete;

        static void init(int& argc, char** argv);
        static void cleanUp();

        static bool active();
        static void proceed();
    };

    void handleXyzArgs(StringMap& args);

    struct OutputColorManagement
    {
        bool applyViewTransform = false;
        std::string colorSpace = "";
        std::string display = "";
        std::string view = "";
        std::string look = "";
        float exposure = 0.0f;

        OutputColorManagement(StringMap& args, const std::string& filename);
        void apply();
    };

    // Called before CmImageIO::readImage()
    void setInputColorSpace(const std::string& colorSpace);

    BOOL WINAPI CtrlHandler(_In_ DWORD dwCtrlType);

}
