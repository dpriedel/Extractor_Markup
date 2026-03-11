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
#include <future>
#include <mutex>
#include <spdlog/spdlog.h>
#include <vector>

class ThreadWorkerPool
{
public:
    explicit ThreadWorkerPool(size_t max_threads);

    template <typename Function>
    void submit_all(const std::vector<std::string> &items, Function func)
    {
        std::vector<std::future<void>> futures;
        futures.reserve(items.size());

        semaphore sem(max_threads_);
        std::atomic<bool> shutdown_requested{false};

        for (const auto &item : items)
        {
            futures.emplace_back(std::async(std::launch::async, [this, &item, &func, &sem, &shutdown_requested]() {
                if (shutdown_requested.load())
                {
                    return;
                }
                sem.acquire();
                try
                {
                    func(item);
                }
                catch (const std::exception &e)
                {
                    spdlog::error("Worker thread exception: {}", e.what());
                }
                sem.release();
            }));
        }

        // Wait for all tasks to complete
        for (auto &f : futures)
        {
            f.get();
        }
    }

    void request_shutdown();

    bool is_shutdown_requested() const;

    size_t max_threads() const;

private:
    class semaphore
    {
    public:
        explicit semaphore(size_t count);

        void acquire();

        void release();

    private:
        std::mutex mutex_;
        std::condition_variable cv_;
        size_t count_;
    };

    size_t max_threads_;
    std::atomic<bool> shutdown_requested_{false};
};

#endif /* THREADWORKERPOOL_H_ */
