//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#ifndef JOSEPH_THREADPOOL_H
#define JOSEPH_THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <stdexcept>

namespace Joseph {
    class ThreadPool {
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> taskQueue;
        mutable std::mutex queueMutex;
        std::condition_variable cv;
        std::atomic<bool> running{true};
    public:
        explicit ThreadPool(size_t numThreads) {
            if (numThreads == 0)
                throw std::invalid_argument("Thread pool size must be at least 1");

            workers.reserve(numThreads);
            for (size_t i = 0; i < numThreads; i++) {
                workers.emplace_back([this] {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(queueMutex);
                            cv.wait(lock, [this] {
                                return !taskQueue.empty() || !running;
                            });

                            if (!running && taskQueue.empty())
                                return;

                            task = std::move(taskQueue.front());
                            taskQueue.pop();
                        }
                        task();
                    }
                });
            }
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        ~ThreadPool() {
            shutdown();
        }

        template<typename F, typename... Args>
        void submit(F&& f, Args&&... args) {
            if (!running)
                throw std::runtime_error("Cannot submit to a stopped thread pool");

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                taskQueue.emplace([func = std::forward<F>(f),
                                   ...capturedArgs = std::forward<Args>(args)]() mutable {
                    func(std::forward<Args>(capturedArgs)...);
                });
            }
            cv.notify_one();
        }

        void shutdown() {
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (!running) return;
                running = false;
            }
            cv.notify_all();
            for (auto& t : workers)
                if (t.joinable()) t.join();
        }

        size_t size() const { return workers.size(); }
        size_t queuedTasks() const {
            std::lock_guard<std::mutex> lock(queueMutex);
            return taskQueue.size();
        }
    };
} // namespace Joseph

#endif //JOSEPH_THREADPOOL_H
