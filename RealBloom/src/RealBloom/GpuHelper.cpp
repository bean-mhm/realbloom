#include "GpuHelper.h"

std::string strFromGpuHelperOperationType(GpuHelperOperationType opType)
{
    if (opType == GpuHelperOperationType::Diffraction)
        return "Diffraction";

    if (opType == GpuHelperOperationType::Dispersion)
        return "Dispersion";

    if (opType == GpuHelperOperationType::ConvNaive)
        return "ConvNaive";

    if (opType == GpuHelperOperationType::ConvFFT)
        return "ConvFFT";

    return "";
}

GpuHelper::GpuHelper()
{
    m_randomNumber = Random::nextU64();

    m_inpFilename = strFormat(
        "%srealbloom_gpu_operation%llu",
        getTempDirectory().c_str(),
        m_randomNumber);

    m_outFilename = m_inpFilename + "out";
}

uint64_t GpuHelper::getRandomNumber()
{
    return m_randomNumber;
}

const std::string& GpuHelper::getInpFilename()
{
    return m_inpFilename;
}

const std::string& GpuHelper::getOutFilename()
{
    return m_outFilename;
}

void GpuHelper::run()
{
    ZeroMemory(&m_startupInfo, sizeof(m_startupInfo));
    m_startupInfo.cb = sizeof(m_startupInfo);
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));

    std::string commandLine = strFormat("\"%s\" \"%s\"", getLocalPath("RealBloomGpuHelper.exe").c_str(), m_inpFilename.c_str());

    if (CreateProcessA(
        NULL,                            // No module name (use command line)
        (char*)(commandLine.c_str()),    // Command line
        NULL,                            // Process handle not inheritable
        NULL,                            // Thread handle not inheritable
        FALSE,                           // Set handle inheritance to FALSE
        CREATE_NO_WINDOW,                // Creation flags
        NULL,                            // Use parent's environment block
        NULL,                            // Use parent's starting directory 
        &m_startupInfo,                  // Pointer to STARTUPINFO structure
        &m_processInfo))                 // Pointer to PROCESS_INFORMATION structure
    {
        m_hasHandles = true;

        // Close when the parent dies
        HANDLE hJobObject = Async::getShared("hJobObject");
        if (hJobObject)
            AssignProcessToJobObject(hJobObject, m_processInfo.hProcess);
    }
    else
    {
        throw std::exception(makeError(
            __FUNCTION__, "", strFormat("CreateProcess failed (%lu).", GetLastError())
        ).c_str());
    }
}

void GpuHelper::waitForOutput(std::function<bool()> mustCancel)
{
    auto t1 = std::chrono::system_clock::now();

    while (!std::filesystem::exists(m_outFilename))
    {
        if (getElapsedMs(t1) > GPU_HELPER_FILE_TIMEOUT)
        {
            killProcess(m_processInfo);
            throw std::exception(makeError(__FUNCTION__, "", "Timeout").c_str());
        }

        if (mustCancel())
        {
            killProcess(m_processInfo);
            throw std::exception(makeError(__FUNCTION__, "", "Canceled").c_str());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIMESTEP_SHORT));
    }
}

bool GpuHelper::isRunning()
{
    return processIsRunning(m_processInfo);
}

void GpuHelper::kill()
{
    if (isRunning()) killProcess(m_processInfo);
}

void GpuHelper::cleanUp()
{
    kill();

    if (m_hasHandles)
    {
        m_hasHandles = false;
        CloseHandle(m_processInfo.hProcess);
        CloseHandle(m_processInfo.hThread);
    }

    if (GPU_HELPER_DELETE_TEMP)
    {
        deleteFile(m_inpFilename);
        deleteFile(m_outFilename);
    }
}
