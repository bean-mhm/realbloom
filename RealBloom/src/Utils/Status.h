#pragma once

#include <string>
#include <chrono>

#include "Misc.h"

class BaseStatus
{
public:
    BaseStatus() {};
    virtual ~BaseStatus() {};

    bool isOK() const;
    const std::string& getError() const;

    virtual void setError(const std::string& message);
    virtual void reset();

protected:
    bool m_ok = true;
    std::string m_error = "";

};

class WorkingStatus : public BaseStatus
{
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

protected:
    bool m_working = false;
    bool m_mustCancel = false;

private:
    typedef BaseStatus super;

};

class TimedWorkingStatus : public WorkingStatus
{
public:
    TimedWorkingStatus() {};
    ~TimedWorkingStatus() {};

    bool hasTimestamps() const;
    float getElapsedSec() const;

    virtual void setWorking() override;
    virtual void setDone() override;

    virtual void setError(const std::string& message) override;
    virtual void reset() override;

protected:
    std::chrono::time_point<std::chrono::system_clock> m_timeStart;
    std::chrono::time_point<std::chrono::system_clock> m_timeEnd;
    bool m_hasTimestamps = false;

private:
    typedef WorkingStatus super;

};
