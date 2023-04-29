#pragma once

#include <iostream>
#include <string>
#include <chrono>

#include "ConsoleColors.h"
#include "Misc.h"

// Scoped Timer for CLI Commands
class CliStackTimer
{
private:
    std::string m_title;
    bool m_isTotal;
    std::chrono::high_resolution_clock::time_point m_start;

public:
    CliStackTimer(const std::string& title, bool isTotal = false);

    CliStackTimer(const CliStackTimer&) = delete;
    CliStackTimer& operator= (const CliStackTimer&) = delete;

    // Must be called at the end of a code block, unless
    // an exception is thrown. Does nothing if verbose
    // is set to false.
    void done(bool verbose);
};
