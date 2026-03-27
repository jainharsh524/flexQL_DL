#include "server/thread_pool.h"
#include <algorithm>

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0)
        num_threads = std::max<size_t>(1, std::thread::hardware_concurrency());
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i)
        workers_.emplace_back([this]{ worker_loop(); });
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lk(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
}

void ThreadPool::worker_loop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [this]{ return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) return;
            task = std::move(queue_.front());
            queue_.pop();
            ++active_;
        }
        task();
        {
            std::unique_lock<std::mutex> lk(mu_);
            --active_;
        }
        cv_done_.notify_all();
    }
}

void ThreadPool::submit(Task task) {
    {
        std::unique_lock<std::mutex> lk(mu_);
        queue_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_done_.wait(lk, [this]{ return queue_.empty() && active_ == 0; });
}
