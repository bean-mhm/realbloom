#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <future>
#include <functional>
#include <unordered_map>
#include <variant>

#include "Utils/Misc.h"

typedef std::variant<void*, bool, int32_t, uint32_t, int64_t, uint64_t, float, double, std::string> SignalValue;

class Async
{
private:
    static std::vector<std::pair<std::string, SignalValue>> S_SIGNALS;
    static std::mutex S_SIGNALS_MUTEX;

    static std::deque<std::packaged_task<void()>> S_JOBS;
    static std::mutex S_JOBS_MUTEX;

public:
    Async() = delete;
    Async(const Async&) = delete;
    Async& operator= (const Async&) = delete;

    static void emitSignal(const std::string& name, const SignalValue& value);
    static bool readSignal(const std::string& name, SignalValue& outValue);
    static bool readSignalLast(const std::string& name, SignalValue& outValue);

    static void scheduleJob(std::function<void()> job, bool wait);
    static void processJobs(const std::string& threadName);

};
