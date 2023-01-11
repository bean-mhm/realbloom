#include "Status.h"

bool BaseStatus::isOK() const
{
    return m_ok;
}

const std::string& BaseStatus::getError() const
{
    return m_error;
}

void BaseStatus::setError(const std::string& message)
{
    m_error = message;
    m_ok = false;
}

void BaseStatus::reset()
{
    m_ok = true;
    m_error = "";
}

bool WorkingStatus::isWorking() const
{
    return m_working;
}

bool WorkingStatus::isCanceling() const
{
    return m_canceling;
}

void WorkingStatus::setWorking()
{
    reset();
    m_working = true;
}

void WorkingStatus::setDone()
{
    m_canceling = false;
    m_working = false;
    m_working.notify_all();
}

void WorkingStatus::cancel()
{
    m_canceling = true;
    m_working.wait(true);
    reset();
}

void WorkingStatus::setError(const std::string& message)
{
    super::setError(message);

    m_canceling = false;
    m_working = false;
    m_working.notify_all();
}

void WorkingStatus::reset()
{
    super::reset();

    m_canceling = false;
    m_working = false;
    m_working.notify_all();
}

bool TimedWorkingStatus::hasTimestamps() const
{
    return m_hasTimestamps;
}

float TimedWorkingStatus::getElapsedSec() const
{
    if (m_working)
        return getElapsedMs(m_timeStart) / 1000.0f;

    if (!m_hasTimestamps)
        return 0.0f;

    return getElapsedMs(m_timeStart, m_timeEnd) / 1000.0f;
}

void TimedWorkingStatus::setWorking()
{
    super::setWorking();
    m_hasTimestamps = false;
    m_timeStart = std::chrono::system_clock::now();
}

void TimedWorkingStatus::setDone()
{
    m_timeEnd = std::chrono::system_clock::now();
    m_hasTimestamps = true;
    super::setDone();
}

void TimedWorkingStatus::setError(const std::string& message)
{
    super::setError(message);
    m_hasTimestamps = false;
}

void TimedWorkingStatus::reset()
{
    super::reset();
    m_hasTimestamps = false;
}
