#include "Async.h"

std::unordered_map<std::string, void*> Async::S_SHARED;
std::mutex Async::S_SHARED_MUTEX;

std::deque<std::packaged_task<void()>> Async::S_TASKS;
std::mutex Async::S_TASKS_MUTEX;

void Async::schedule(std::function<void()> job)
{
    std::packaged_task<void()> task(job);
    {
        std::lock_guard<std::mutex> lock(S_TASKS_MUTEX);
        S_TASKS.push_back(std::move(task));
    }
}

void Async::putShared(std::string key, void* value)
{
    std::lock_guard<std::mutex> lock(S_SHARED_MUTEX);
    S_SHARED[key] = value;
}

void* Async::getShared(std::string key)
{
    if (S_SHARED.contains(key))
        return S_SHARED[key];
    return nullptr;
}
