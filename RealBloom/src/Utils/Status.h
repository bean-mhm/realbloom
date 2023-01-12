#pragma once

#include <string>
#include <chrono>
#include <thread>
#include <atomic>

#include "Misc.h"

class BaseStatus
{
protected:
    bool m_ok = true;
    std::string m_error = "";

public:
    BaseStatus() {};
    virtual ~BaseStatus() {};

    bool isOK() const;
    const std::string& getError() const;

    virtual void setError(const std::string& message);
    virtual void reset();

};

class WorkingStatus : public BaseStatus
{
private:
    typedef BaseStatus super;

protected:
    bool m_working = false;
    bool m_mustCancel = false;

public:
    WorkingStatus() {};
    ~WorkingStatus() {};

    bool isWorking() const;
    bool mustCancel() const;

    virtual void setWorking();
    virtual void setDone();
    virtual void setMustCancel();

    virtual void setError(const std::string& message) override;
    virtual void reset() override;

};

class TimedWorkingStatus : public WorkingStatus
{
private:
    typedef WorkingStatus super;

protected:
    std::chrono::time_point<std::chrono::system_clock> m_timeStart;
    std::chrono::time_point<std::chrono::system_clock> m_timeEnd;
    bool m_hasTimestamps = false;

public:
    TimedWorkingStatus() {};
    ~TimedWorkingStatus() {};

    bool hasTimestamps() const;
    float getElapsedSec() const;

    virtual void setWorking() override;
    virtual void setDone() override;

    virtual void setError(const std::string& message) override;
    virtual void reset() override;

};
