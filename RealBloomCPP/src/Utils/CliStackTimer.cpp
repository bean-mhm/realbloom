#include "CliStackTimer.h"

CliStackTimer::CliStackTimer(const std::string& title, bool isTotal)
    : m_title(title), m_isTotal(isTotal)
{
    m_start = std::chrono::high_resolution_clock::now();
}

void CliStackTimer::done(bool verbose)
{
    if (!verbose) return;

    std::chrono::nanoseconds elapsedNs = std::chrono::high_resolution_clock::now() - m_start;
    double elapsedMs = (double)elapsedNs.count() / 1000000.0;

    if (m_isTotal)
        std::cout
        << consoleColor(COL_PRI)
        << "Total: "
        << consoleColor(COL_SEC)
        << strFormat("%.3f ms", elapsedMs)
        << consoleColor() << "\n";
    else
        std::cout
        << consoleColor(COL_SEC)
        << strRightPadding(strFormat("%.3f ms", elapsedMs), 12)
        << consoleColor()
        << m_title << "\n";
}
