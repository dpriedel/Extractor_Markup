#/* =====================================================================================
 *
 * Filename:  ThreadWorkerPool.cpp
 *
 * Description:  Provides a managed thread pool with a limit on number of concurrent threads.
 *
 * Version:  1.0
 * Created:  2026-03-11 16:10:56
 * Revision:  none
 * Compiler:  gcc / g++
 *
 * Author:  David P. Riedel <driedel@cox.net> and Qwen3.5
 * Copyright (c) 2026, David P. Riedel
 *
 * =====================================================================================
 */

#include "ThreadWorkerPool.h"

ThreadWorkerPool::ThreadWorkerPool(size_t max_threads) : max_threads_(max_threads)
{
    if (max_threads_ == 0)
    {
        max_threads_ = std::thread::hardware_concurrency();
    }
    spdlog::info("Thread pool initialized with {} workers.", max_threads_);
}

void ThreadWorkerPool::request_shutdown()
{
    shutdown_requested_.store(true);
}

bool ThreadWorkerPool::is_shutdown_requested() const
{
    return shutdown_requested_.load();
}

size_t ThreadWorkerPool::max_threads() const
{
    return max_threads_;
}

// Explicit template instantiation
template void ThreadWorkerPool::submit_all(const std::vector<std::string> &, void (*)(const std::string &));

ThreadWorkerPool::semaphore::semaphore(size_t count) : count_(count)
{
}

void ThreadWorkerPool::semaphore::acquire()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return count_ > 0; });
    --count_;
}

void ThreadWorkerPool::semaphore::release()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++count_;
    }
    cv_.notify_one();
}
