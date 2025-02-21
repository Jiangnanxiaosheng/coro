//
// Created by JYH on 2025-02-21.
//

#include <iostream>
#include "coro/task.hpp"


#include <coroutine>
#include <thread>
#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>

class ThreadPool {
public:
    ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency()) {
        m_threads.reserve(thread_count);
        for (int i = 0; i < thread_count; ++i) {
            m_threads.emplace_back([this]() {
                executor();
            });
        }
    }

    ~ThreadPool() { shutdown(); }

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    class Awaiter {
    public:
        explicit Awaiter(ThreadPool& p) : pool(p) {}
        auto await_ready()  noexcept { return false; }
        // 在协程内部调用co_await tp.schedule(), 此处会将当前协程，即 awaiting_coroutine 放入ThreadPool的工作队列中
        auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept {
            handle = awaiting_coroutine;
            pool.scheduler_impl(handle);
        }
        auto await_resume() noexcept {}

    private:
        ThreadPool& pool;
        std::coroutine_handle<> handle;
    };

public:
    Awaiter schedule() {
        m_size.fetch_add(1, std::memory_order_release);
        if (m_stop.load(std::memory_order_acquire) == false) {
            return Awaiter{ *this };
        }
        else {
            m_size.fetch_sub(1, std::memory_order_release);
            throw std::runtime_error("coro::ThreadPool is shutting down, unable to schedule new tasks.");
        }
    }

    // 允许当前任务主动让出执行权，重新排队到末尾，以便其他任务有机会执行，有助于避免长任务独占线程
    Awaiter yield() {
        return schedule();
    }

    // 返回线程池的大小
    std::size_t thread_count() const {
        return m_threads.size();
    }

    // 任务队列中等待的任务数量 + 正在执行的任务。
    std::size_t size() {
        return m_size.load(std::memory_order_acquire);
    }


private:
    void executor() {
        while (m_stop.load(std::memory_order_acquire) == false) {
            std::unique_lock<std::mutex> lk {m_queue_mutex};
            m_cv.wait(lk, [this]() {
                return m_stop.load(std::memory_order_acquire) || !m_queue.empty();
            });

            if (!m_queue.empty()) {
                auto handle = m_queue.front();
                m_queue.pop();
                lk.unlock();

                handle.resume();
                m_size.fetch_sub(1, std::memory_order_release);
            }
        }

        while (m_size.load(std::memory_order_acquire) > 0) {
            std::unique_lock<std::mutex> lk {m_queue_mutex};

            if (m_queue.empty()) {
                break;
            }

            auto handle = m_queue.front();
            m_queue.pop();
            lk.unlock();

            handle.resume();
            m_size.fetch_sub(1, std::memory_order_release);
        }

    }

    void shutdown() noexcept {
        if (m_stop.exchange(true, std::memory_order_acq_rel) == false) {
            m_cv.notify_all();

            for (auto& thread : m_threads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        }
    }

    void scheduler_impl(std::coroutine_handle<> handle) noexcept {
        if (handle == nullptr || handle.done()) {
            return;
        }

        {
            std::scoped_lock lk {m_queue_mutex};
            m_queue.emplace(handle);
            m_cv.notify_one();
        }
    }


    std::vector<std::thread> m_threads;
    std::queue<std::coroutine_handle<>> m_queue;
    std::mutex m_queue_mutex;
    std::atomic<bool> m_stop { false };
    std::condition_variable m_cv;

    // 任务队列中等待的任务数量 + 正在执行的任务。
    std::atomic<std::size_t> m_size {0};
};


int main() {

    ThreadPool pool(4);
    std::cout << "main函数所在线程: " << std::this_thread::get_id() << "\n";

    auto task = [&pool]() -> Task<int> {
        co_await pool.schedule();

        std::cout << "协程任务开始\n";
        std::cout << "协程函数所在线程: " << std::this_thread::get_id() << "\n";
        std::cout << "协程任务结束\n";
        co_return 199;
    };

    auto t = task();
    if (!t.done()) {
        t.resume();
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "main函数结束\n";
    return 0;

}