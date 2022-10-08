#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <future>
#include <thread>
#include <functional>
#include <unordered_map>

class Async
{
private:
    static std::unordered_map<std::string, void*> s_shared;
    static std::mutex s_sharedMutex;

public:
    static std::deque<std::packaged_task<void()>> s_tasks;
    static std::mutex s_tasksMutex;
    static void schedule(std::function<void()> job);

    static void putShared(std::string key, void* value);
    static void* getShared(std::string key);
};