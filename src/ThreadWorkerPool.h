/* =====================================================================================
 *
 * Filename:  ThreadWorkerPool.h
 *
 * Description:  Provides a managed thread pool with a limit on number of concurrent threads.
 *
 * Version:  1.0
 * Created:  2026-03-11 16:07:58
 * Revision:  none
 * Compiler:  gcc / g++
 *
 * Author:  David P. Riedel <driedel@cox.net> and Qwen3.5
 * Copyright (c) 2026, David P. Riedel
 *
 * =====================================================================================
 */

#ifndef THREADWORKERPOOL_H_
#define THREADWORKERPOOL_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>

class ThreadWorkerPool
{
public:
    explicit ThreadWorkerPool(size_t max_threads);
    ~ThreadWorkerPool();

    // Deleted copy/move to prevent accidental double-shutdowns
    ThreadWorkerPool(const ThreadWorkerPool &) = delete;
    ThreadWorkerPool &operator=(const ThreadWorkerPool &) = delete;

    /**
     * Submits a batch of items to the pool and waits for them to complete.
     */
    template <typename Function>
    void submit_all(const std::vector<std::string> &items, Function func)
    {
        std::atomic<size_t> remaining{items.size()};
        std::mutex wait_mutex;
        std::condition_variable wait_cv;

        for (const auto &item : items)
        {
            // Capture item by value to ensure thread safety
            enqueue_task([this, item, &func, &remaining, &wait_cv]() {
                if (!is_shutdown_requested())
                {
                    try
                    {
                        func(item);
                    }
                    catch (const std::exception &e)
                    {
                        spdlog::error("Worker thread exception: {}", e.what());
                    }
                }

                // Signal completion of this specific task
                if (--remaining == 0)
                {
                    wait_cv.notify_all();
                }
            });
        }

        // Block until this specific batch is done
        std::unique_lock<std::mutex> lock(wait_mutex);
        wait_cv.wait(lock, [&] { return remaining == 0 || is_shutdown_requested(); });
    }

    void request_shutdown();
    bool is_shutdown_requested() const;
    size_t max_threads() const;

private:
    void worker_loop();
    void enqueue_task(std::function<void()> task);

    size_t max_threads_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_requested_{false};
};

#endif /* THREADWORKERPOOL_H_ */
