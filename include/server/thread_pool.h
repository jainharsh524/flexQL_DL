#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <cstddef>

// ─────────────────────────────────────────────────────────────────────────────
// THREAD POOL
//
// Fixed set of worker threads consuming tasks from a shared queue.
// Workers block on condition_variable when the queue is empty.
// ─────────────────────────────────────────────────────────────────────────────
class ThreadPool {
public:
    using Task = std::function<void()>;

    // num_threads = 0 → use hardware_concurrency
    explicit ThreadPool(size_t num_threads = 0);
    ~ThreadPool();

    // Submit a task; returns immediately (fire-and-forget).
    void submit(Task task);

    // Wait until all submitted tasks have finished.
    void wait_all();

    size_t thread_count() const { return workers_.size(); }
    size_t queue_depth() const {
        std::unique_lock<std::mutex> lk(mu_);
        return queue_.size();
    }

private:
    void worker_loop();

    std::vector<std::thread>        workers_;
    std::queue<Task>                queue_;
    mutable std::mutex              mu_;
    std::condition_variable         cv_;
    std::condition_variable         cv_done_;
    std::atomic<bool>               stop_{false};
    std::atomic<size_t>             active_{0};  // Tasks currently executing
};
