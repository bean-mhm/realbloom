#include "Async.h"

std::unordered_map<std::string, void*> Async::s_shared;
std::mutex Async::s_sharedMutex;

std::deque<std::packaged_task<void()>> Async::s_tasks;
std::mutex Async::s_tasksMutex;

void Async::schedule(std::function<void()> job)
{
    std::packaged_task<void()> task(job);
    {
        std::lock_guard<std::mutex> lock(s_tasksMutex);
        s_tasks.push_back(std::move(task));
    }
}

void Async::putShared(std::string key, void* value)
{
    std::lock_guard<std::mutex> lock(s_sharedMutex);
    s_shared[key] = value;
}

void* Async::getShared(std::string key)
{
    if (s_shared.contains(key))
        return s_shared[key];
    return nullptr;
}
