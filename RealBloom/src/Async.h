#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <future>
#include <functional>
#include <unordered_map>

class Async
{
private:
    static std::unordered_map<std::string, void*> S_SHARED;
    static std::mutex S_SHARED_MUTEX;

public:
    static std::deque<std::packaged_task<void()>> S_TASKS;
    static std::mutex S_TASKS_MUTEX;
    static void schedule(std::function<void()> job);

    static void putShared(std::string key, void* value);
    static void* getShared(std::string key);
};
