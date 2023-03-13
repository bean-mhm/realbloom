#include "Async.h"

std::vector<std::pair<std::string, SignalValue>> Async::S_SIGNALS;
std::mutex Async::S_SIGNALS_MUTEX;

std::deque<std::packaged_task<void()>> Async::S_TASKS;
std::mutex Async::S_TASKS_MUTEX;

void Async::emitSignal(const std::string& name, const SignalValue& value)
{
    std::scoped_lock lock(S_SIGNALS_MUTEX);
    S_SIGNALS.push_back({ name, value });
}

bool Async::readSignal(const std::string& name, SignalValue& outValue)
{
    std::scoped_lock lock(S_SIGNALS_MUTEX);
    for (size_t i = 0; i < S_SIGNALS.size(); i++)
    {
        const auto& signal = S_SIGNALS[i];
        if (signal.first == name)
        {
            outValue = signal.second;
            S_SIGNALS.erase(S_SIGNALS.begin() + i);
            return true;
        }
    }
    return false;
}

bool Async::readSignalLast(const std::string& name, SignalValue& outValue)
{
    bool found = false;

    while (readSignal(name, outValue))
        found = true;

    return found;
}

void Async::schedule(std::function<void()> job)
{
    std::packaged_task<void()> task(job);
    std::scoped_lock lock(S_TASKS_MUTEX);
    S_TASKS.push_back(std::move(task));
}
