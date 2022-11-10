#include "Misc.h"

std::string printErr(const std::string& source, const std::string& stage, const std::string& message)
{
    std::string s = stringFormat("[%s] %s: %s", source.c_str(), stage.c_str(), message.c_str());
    std::cerr << s << "\n";
    return s;
}

std::string printErr(const std::string& source, const std::string& message)
{
    std::string s = stringFormat("[%s] %s", source.c_str(), message.c_str());
    std::cerr << s << "\n";
    return s;
}


std::string stringFromDuration(float seconds)
{
    if (seconds < 60.0f)
    {
        return stringFormat("%.1fs", seconds);
    } else
    {
        uint32_t intSec = (int)floorf(seconds);

        uint32_t intHr = intSec / 3600;
        uint32_t intMin = (intSec / 60) % 60;
        intSec %= 60;

        if (intHr > 0)
        {
            return stringFormat("%dh %dm %ds", intHr, intMin, intSec);
        } else
        {
            return stringFormat("%dm %ds", intMin, intSec);
        }
    }
}

std::string stringFromDuration2(float seconds)
{
    uint32_t intSec = (int)floorf(seconds);

    uint32_t intHr = intSec / 3600;
    uint32_t intMin = (intSec / 60) % 60;
    intSec %= 60;

    return stringFormat("%02d:%02d:%02d", intHr, intMin, intSec);
}

std::string stringFromSize(uint64_t sizeBytes)
{
    static const char* suffixes[]{ "bytes", "KB", "MB", "GB", "TB" };
    static double powers[]
    {
        pow(1024, 0), // 1 byte
        pow(1024, 1), // 1 KB
        pow(1024, 2), // 1 MB
        pow(1024, 3), // 1 GB
        pow(1024, 4)  // 1 TB
    };

    uint64_t mag = (uint64_t)fmin(4, fmax(0, floor(log((double)sizeBytes) / log(1024.0))));
    if (mag == 0)
        return stringFormat("%lu %s", sizeBytes, suffixes[mag]);
    else
        return stringFormat("%.1f %s", (double)sizeBytes / powers[mag], suffixes[mag]);
}

std::string stringFromBigNumber(uint64_t bigNumber)
{
    static const char* suffixes[]{ "", "K", "M", "B", "T" };
    static double powers[]
    {
        pow(1000, 0), // 1
        pow(1000, 1), // 1K
        pow(1000, 2), // 1M
        pow(1000, 3), // 1B
        pow(1000, 4)  // 1T
    };

    uint64_t mag = (uint64_t)fmin(4, fmax(0, floor(log((double)bigNumber) / log(1000.0))));
    if (mag == 0)
        return stringFormat("%lu", bigNumber);
    else
        return stringFormat("%.1f%s", (double)bigNumber / powers[mag], suffixes[mag]);
}

uint32_t getElapsedMs(std::chrono::system_clock::time_point startTime)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count();
}

HANDLE createMutex(const std::string& name)
{
    return CreateMutexA(
        NULL,              // default security descriptor
        FALSE,             // mutex not owned
        name.c_str());     // object name
}

HANDLE openMutex(const std::string& name)
{
    return OpenMutexA(
        MUTEX_ALL_ACCESS,  // request full access
        FALSE,             // handle not inheritable
        name.c_str());     // object name
}

void waitForMutex(HANDLE hMutex)
{
    if (hMutex != NULL)
        WaitForSingleObject(hMutex, INFINITE);
}

void releaseMutex(HANDLE hMutex)
{
    if (hMutex != NULL)
        ReleaseMutex(hMutex);
}

void closeMutex(HANDLE hMutex)
{
    if (hMutex != NULL)
        CloseHandle(hMutex);
}

void killProcess(PROCESS_INFORMATION pi)
{
    if (TerminateProcess(pi.hProcess, 1))
        WaitForSingleObject(pi.hProcess, INFINITE);
}

bool processIsRunning(PROCESS_INFORMATION pi)
{
    DWORD exitCode;
    if (GetExitCodeProcess(pi.hProcess, &exitCode))
        return exitCode == STILL_ACTIVE;
    return false;
}

bool deleteFile(const std::string& filename)
{
    if (std::filesystem::exists(filename))
        return std::filesystem::remove(filename);
    return true;
}

void getTempDirectory(std::string& outDir)
{
    char buf[1024];
    if (GetTempPathA(sizeof(buf), buf))
        outDir = buf;
    else
        outDir = std::filesystem::temp_directory_path().string();
}

void openURL(std::string url)
{
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}