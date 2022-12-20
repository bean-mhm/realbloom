#include "Misc.h"

bool printErrEnbaled = true;

std::string printErr(const std::string& source, const std::string& stage, const std::string& message)
{
    std::string s = strFormat("[%s] %s: %s", source.c_str(), stage.c_str(), message.c_str());
    if (printErrEnbaled) std::cerr << s << "\n";
    return s;
}

std::string printErr(const std::string& source, const std::string& message)
{
    std::string s = strFormat("[%s] %s", source.c_str(), message.c_str());
    if (printErrEnbaled) std::cerr << s << "\n";
    return s;
}

void disablePrintErr()
{
    printErrEnbaled = false;
}

uint32_t getMaxNumThreads()
{
    static uint32_t v = 1;
    static bool firstCall = true;
    if (firstCall)
    {
        firstCall = false;
        v = std::max(1u, std::thread::hardware_concurrency());
    }
    return v;
}

uint32_t getDefNumThreads()
{
    static uint32_t v = 1;
    static bool firstCall = true;
    if (firstCall)
    {
        firstCall = false;
        v = std::max(1u, getMaxNumThreads() / 2);
    }
    return v;
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

std::string getPathSeparator()
{
    return "\\";
}

std::string getExecDir()
{
    static std::string execDir = "";
    if (execDir.empty())
    {
        char path_cstr[1024] = { 0 };
        GetModuleFileNameA(NULL, path_cstr, 1024);

        auto path = std::filesystem::path(std::string(path_cstr)).parent_path();
        execDir = std::filesystem::canonical(path).string();
        
        if (!execDir.ends_with(getPathSeparator()))
            execDir += getPathSeparator();

        execDir = std::filesystem::path(execDir).make_preferred().string();
    }
    return execDir;
}

std::string getLocalPath(const std::string& path)
{
    return getExecDir() + path;
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

void SimpleState::setError(const std::string& message)
{
    m_ok = false;
    m_error = message;
}

void SimpleState::setOk()
{
    m_ok = true;
    m_error = "";
}

bool SimpleState::ok() const
{
    return m_ok;
}

std::string SimpleState::getError() const
{
    return m_error;
}
