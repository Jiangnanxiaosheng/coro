//
// Created by JYH on 2025-02-24.
//

#ifndef CORO_TIMERSCHEDULER_HPP
#define CORO_TIMERSCHEDULER_HPP

#include <coroutine>
#include <queue>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>


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

#endif //CORO_TIMERSCHEDULER_HPP
