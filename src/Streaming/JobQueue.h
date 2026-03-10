// src/Streaming/JobQueue.h
//
// Phase 4 Step 4.2.1 — Thread-safe job queue backed by a std::thread pool.
// The main thread pushes "Load Chunk" jobs; background threads read heightmaps
// and scene data into memory buffers without blocking the game loop.

#ifndef ENGINE_JOBQUEUE_H
#define ENGINE_JOBQUEUE_H

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>

class JobQueue {
public:
    /// @param numThreads  Number of worker threads (default: 2).
    explicit JobQueue(unsigned int numThreads = 2) : shutdown_(false) {
        for (unsigned int i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this]() { workerLoop(); });
        }
    }

    ~JobQueue() {
        stop();
    }

    /// Push a job to be executed on a background thread.
    void push(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push(std::move(job));
        }
        cv_.notify_one();
    }

    /// Returns the number of pending jobs.
    size_t pendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return jobs_.size();
    }

    /// Stop all worker threads.  Blocks until all running jobs finish.
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() {
                    return shutdown_ || !jobs_.empty();
                });
                if (shutdown_ && jobs_.empty()) return;
                job = std::move(jobs_.front());
                jobs_.pop();
            }
            job();
        }
    }

    std::vector<std::thread>           workers_;
    std::queue<std::function<void()>>  jobs_;
    mutable std::mutex                 mutex_;
    std::condition_variable            cv_;
    std::atomic<bool>                  shutdown_;
};

#endif // ENGINE_JOBQUEUE_H
