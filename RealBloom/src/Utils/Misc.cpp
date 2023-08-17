#include "Misc.h"

static std::function<void(std::string)> g_printHanlder = [](std::string s) { std::cout << s << "\n"; };

std::string makeError(const std::string& source, const std::string& stage, const std::string& message, bool print)
{
    std::string s = "";

    if (!source.empty())
        s += strFormat("[%s] ", source.c_str());

    if (!stage.empty())
        s += strFormat("%s: ", stage.c_str());

    s += message;

    if (print) g_printHanlder(s);

    return s;
}

void printError(const std::string& source, const std::string& stage, const std::string& message)
{
    makeError(source, stage, message, true);
}

void printWarning(const std::string& source, const std::string& stage, const std::string& message)
{
    makeError(source, stage, message, true);
}

void printInfo(const std::string& source, const std::string& stage, const std::string& message)
{
    makeError(source, stage, message, true);
}

void setPrintHandler(std::function<void(std::string)> handler)
{
    g_printHanlder = handler;
}

uint32_t getMaxNumThreads()
{
    static uint32_t v = 1;
    static bool init = true;
    if (init)
    {
        init = false;
        v = std::max(1u, std::thread::hardware_concurrency());
    }
    return v;
}

uint32_t getDefNumThreads()
{
    static uint32_t num = 1;
    static bool init = true;
    if (init)
    {
        init = false;
        num = std::max(1u, getMaxNumThreads() / 2);
    }
    return num;
}

float getElapsedMs(std::chrono::system_clock::time_point startTime)
{
    auto duration = std::chrono::system_clock::now() - startTime;
    uint64_t elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    return (float)elapsedNs / 1000000.0f;
}

float getElapsedMs(std::chrono::system_clock::time_point startTime, std::chrono::system_clock::time_point endTime)
{
    auto duration = endTime - startTime;
    uint64_t elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    return (float)elapsedNs / 1000000.0f;
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

void closeMutex(HANDLE& hMutex)
{
    if (hMutex != NULL)
    {
        CloseHandle(hMutex);
        hMutex = NULL;
    }
}

const std::string& getPathSeparator()
{
    static std::string pathSeparator = "\\";
    return pathSeparator;
}

const std::string& getExecDir()
{
    static std::string execDir = "";

    if (execDir.empty())
    {
        char path_cstr[2048] = { 0 };
        GetModuleFileNameA(NULL, path_cstr, 2048);

        auto path = std::filesystem::path(std::string(path_cstr)).parent_path();
        execDir = std::filesystem::canonical(path).string();

        if (!execDir.ends_with(getPathSeparator()))
            execDir += getPathSeparator();

        execDir = std::filesystem::path(execDir).make_preferred().string();
    }

    return execDir;
}

const std::string& getTempDirectory()
{
    static std::string tempDir = "";

    if (tempDir.empty())
    {
        char path_cstr[2048] = { 0 };
        if (GetTempPathA(2048, path_cstr))
            tempDir = path_cstr;
        else
            tempDir = std::filesystem::temp_directory_path().string();
    }

    return tempDir;
}

std::string getLocalPath(const std::string& path)
{
    return getExecDir() + path;
}

std::string getFileExtension(const std::string& filename)
{
    return strLowercase(std::filesystem::path(filename).extension().string());
}


bool deleteFile(const std::string& filename)
{
    if (std::filesystem::exists(filename))
        return std::filesystem::remove(filename);
    return true;
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

void openURL(std::string url)
{
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}
