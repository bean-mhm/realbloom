#include "Async.h"

std::vector<std::pair<std::string, SignalValue>> Async::S_SIGNALS;
std::mutex Async::S_SIGNALS_MUTEX;

std::deque<std::packaged_task<void()>> Async::S_JOBS;
std::mutex Async::S_JOBS_MUTEX;

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

void Async::scheduleJob(std::function<void()> job, bool wait)
{
    std::packaged_task<void()> task(job);
    auto future = task.get_future();

    {
        std::scoped_lock lock(S_JOBS_MUTEX);
        S_JOBS.push_back(std::move(task));
    }

    if (wait)
    {
        future.get();
    }
}

void Async::processJobs(const std::string& threadName)
{
    std::unique_lock<std::mutex> lock(S_JOBS_MUTEX);

    while (!Async::S_JOBS.empty())
    {
        auto task(std::move(S_JOBS.front()));
        S_JOBS.pop_front();

        // Retrieve the associated future
        auto future = task.get_future();

        // Unlock during the job
        lock.unlock();
        try
        {
            task();
            future.get();
        }
        catch (const std::exception& e)
        {
            printError(strFormat("%s (Thread: \"%s\")", __FUNCTION__, threadName.c_str()), "", e.what());
        }
        lock.lock();
    }
}
