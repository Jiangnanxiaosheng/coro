//
// Created by JYH on 2025-02-24.
//
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <coroutine>
#include <chrono>
#include <queue>
#include <condition_variable>

#include "coro/task.hpp"
#include "coro/sync_wait.hpp"
#include "coro/when_all.hpp"


using namespace std::chrono_literals;

struct TimerEvent {
    std::chrono::steady_clock::time_point expiry_time;
    std::coroutine_handle<> handle;

    // 用于优先队列排序（按时间升序）
    bool operator<(const TimerEvent& other) const {
        return expiry_time > other.expiry_time;
    }
};

class TimerScheduler {
public:
    TimerScheduler() {
        // 启动后台调度线程
        m_thread = std::thread([this] { run(); });
    }

    ~TimerScheduler() {
        m_stop.store(true, std::memory_order_release);
        m_cv.notify_all();

        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    // 添加定时事件
    void schedule_until(std::chrono::steady_clock::time_point expiry, std::coroutine_handle<> h) {
        std::unique_lock lock(m_mutex);
        m_events.push(TimerEvent{ expiry, h });
        m_cv.notify_one(); // 通知调度线程有新事件
    }

    void set_stop_true() {
        m_stop.store(true, std::memory_order_release);
    }

private:
    void run() {
        while (!m_stop.load(std::memory_order_acquire)) {
            std::unique_lock lk(m_mutex);
            m_cv.wait(lk, [this]() {
                return m_stop.load(std::memory_order_acquire) || !m_events.empty();
            });

            if (!m_events.empty()) {
                auto next_time = m_events.top().expiry_time;
                // 等待直到最近的事件到期或新事件到达
                if (m_cv.wait_until(lk, next_time) == std::cv_status::timeout) {
                    // 处理到期事件
                    auto event = m_events.top();
                    m_events.pop();
                    lk.unlock();
                    event.handle.resume(); // 恢复协程
                }
            }
        }

        while (!m_events.empty()) {
            std::unique_lock lk(m_mutex);
            auto next_time = m_events.top().expiry_time;
            // 等待直到最近的事件到期或新事件到达
            if (m_cv.wait_until(lk, next_time) == std::cv_status::timeout) {
                // 处理到期事件
                auto event = m_events.top();
                m_events.pop();
                lk.unlock();
                event.handle.resume(); // 恢复协程
            }

        }
    }

    std::priority_queue<TimerEvent> m_events;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
};

// 全局定时器调度器实例
TimerScheduler& getTimerScheduler() {
    static TimerScheduler timer_scheduler;
    return timer_scheduler;
}


struct SleepAwaiter {
    std::chrono::steady_clock::time_point expiry;

    SleepAwaiter(std::chrono::steady_clock::time_point t)
            : expiry(t) {}

    bool await_ready() const {
        return std::chrono::steady_clock::now() >= expiry;
    }

    void await_suspend(std::coroutine_handle<> h) {
        // 注册到定时器调度器
        getTimerScheduler().schedule_until(expiry, h);
    }

    void await_resume() {}
};


inline auto sleep_for(std::chrono::steady_clock::duration delay) {
    return SleepAwaiter(std::chrono::steady_clock::now() + delay);
}

inline auto sleep_until(std::chrono::steady_clock::time_point time) {
    return SleepAwaiter(time);
}

int main() {
    std::cout << "main线程 " << std::this_thread::get_id() << "\n";


    auto task1 = []() -> Task<int> {
        std::cout << "Task1: Start, sleep 5s\n";
        std::cout << "task1线程 " << std::this_thread::get_id() << "\n";
        co_await sleep_for(std::chrono::seconds(5));
        std::cout << "Task1: Resume after 5s\n";

        co_return 1;
    };

    auto task2 = []() -> Task<int> {
        std::cout << "Task2: Start, sleep 8s\n";
        std::cout << "task2线程 " << std::this_thread::get_id() << "\n";
        co_await sleep_for(8s);
        std::cout << "Task2: Resume after 8s\n";

        co_return 2;
    };

    auto task3 = []() -> Task<> {
        std::cout << "Task3: Start, after 6s set m_stop = true\n";
        std::cout << "task3线程 " << std::this_thread::get_id() << "\n";
        co_await sleep_for(6s);
        getTimerScheduler().set_stop_true();
        std::cout << "Task3: Resume after 6s\n";

        co_return;
    };

    auto task4 = [=]() -> Task<> {
        std::cout << "Task4: Start, 等待中\n";
        std::cout << "task4线程 " << std::this_thread::get_id() << "\n";
        auto startTime = std::chrono::steady_clock::now();
        auto [result1, result2, x] = co_await when_all(task1(), task2(), task3());
        auto endTime = std::chrono::steady_clock::now();
        double duration_second = std::chrono::duration<double>(endTime-startTime).count();
        std::cout << "Task4: 等待完成, 等待用时: " << duration_second << "s\n";

        std::cout << "task1 result: " << result1 << " " << "task2 result: " << result2 << "\n";
    };

    sync_wait(task4());

    return 0;
}

/*
 * @ result
    main线程 1
    Task4: Start, 等待中
            task4线程 1
    Task1: Start, sleep 5s
    task1线程 1
    Task2: Start, sleep 8s
    task2线程 1
    Task3: Start, after 6s set m_stop = true
    task3线程 1
    Task1: Resume after 5s
    Task3: Resume after 6s
    Task2: Resume after 8s
    Task4: 等待完成, 等待用时: 8.01385s
    task1 result: 1 task2 result: 2
*/