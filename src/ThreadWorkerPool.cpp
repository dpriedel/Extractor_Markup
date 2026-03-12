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
    for (size_t i = 0; i < max_threads_; ++i)
    {
        workers_.emplace_back(&ThreadWorkerPool::worker_loop, this);
    }
    spdlog::info("Thread pool initialized with {} workers.", max_threads_);
}

ThreadWorkerPool::~ThreadWorkerPool()
{
    request_shutdown();
    for (std::thread &worker : workers_)
    {
        if (worker.joinable())
            worker.join();
    }
}

void ThreadWorkerPool::enqueue_task(std::function<void()> task)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.push(std::move(task));
    }
    condition_.notify_one();
}

void ThreadWorkerPool::worker_loop()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return shutdown_requested_.load() || !tasks_.empty(); });

            if (shutdown_requested_.load())
            {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

void ThreadWorkerPool::request_shutdown()
{
    shutdown_requested_.store(true);

    // 1. Wake up the worker threads to abort
    condition_.notify_all();

    // 2. Wake up the main thread if it is waiting on a batch
    std::lock_guard<std::mutex> lock(batch_cv_mutex_);
    if (active_batch_cv_)
    {
        active_batch_cv_->notify_all();
    }
}

bool ThreadWorkerPool::is_shutdown_requested() const
{
    return shutdown_requested_.load();
}
size_t ThreadWorkerPool::max_threads() const
{
    return max_threads_;
}
