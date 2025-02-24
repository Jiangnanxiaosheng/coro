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

    // �������ȶ������򣨰�ʱ������
    bool operator<(const TimerEvent& other) const {
        return expiry_time > other.expiry_time;
    }
};

class TimerScheduler {
public:
    TimerScheduler() {
        // ������̨�����߳�
        m_thread = std::thread([this] { run(); });
    }

    ~TimerScheduler() {
        m_stop.store(true, std::memory_order_release);
        m_cv.notify_all();

        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    // ��Ӷ�ʱ�¼�
    void schedule_until(std::chrono::steady_clock::time_point expiry, std::coroutine_handle<> h) {
        std::unique_lock lock(m_mutex);
        m_events.push(TimerEvent{ expiry, h });
        m_cv.notify_one(); // ֪ͨ�����߳������¼�
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
                // �ȴ�ֱ��������¼����ڻ����¼�����
                if (m_cv.wait_until(lk, next_time) == std::cv_status::timeout) {
                    // �������¼�
                    auto event = m_events.top();
                    m_events.pop();
                    lk.unlock();
                    event.handle.resume(); // �ָ�Э��
                }
            }
        }

        while (!m_events.empty()) {
            std::unique_lock lk(m_mutex);
            auto next_time = m_events.top().expiry_time;
            // �ȴ�ֱ��������¼����ڻ����¼�����
            if (m_cv.wait_until(lk, next_time) == std::cv_status::timeout) {
                // �������¼�
                auto event = m_events.top();
                m_events.pop();
                lk.unlock();
                event.handle.resume(); // �ָ�Э��
            }

        }
    }

    std::priority_queue<TimerEvent> m_events;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
};

// ȫ�ֶ�ʱ��������ʵ��
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
        // ע�ᵽ��ʱ��������
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
    std::cout << "main�߳� " << std::this_thread::get_id() << "\n";


    auto task1 = []() -> Task<int> {
        std::cout << "Task1: Start, sleep 5s\n";
        std::cout << "task1�߳� " << std::this_thread::get_id() << "\n";
        co_await sleep_for(std::chrono::seconds(5));
        std::cout << "Task1: Resume after 5s\n";

        co_return 1;
    };

    auto task2 = []() -> Task<int> {
        std::cout << "Task2: Start, sleep 8s\n";
        std::cout << "task2�߳� " << std::this_thread::get_id() << "\n";
        co_await sleep_for(8s);
        std::cout << "Task2: Resume after 8s\n";

        co_return 2;
    };

    auto task3 = []() -> Task<> {
        std::cout << "Task3: Start, after 6s set m_stop = true\n";
        std::cout << "task3�߳� " << std::this_thread::get_id() << "\n";
        co_await sleep_for(6s);
        getTimerScheduler().set_stop_true();
        std::cout << "Task3: Resume after 6s\n";

        co_return;
    };

    auto task4 = [=]() -> Task<> {
        std::cout << "Task4: Start, �ȴ���\n";
        std::cout << "task4�߳� " << std::this_thread::get_id() << "\n";
        auto startTime = std::chrono::steady_clock::now();
        auto [result1, result2, x] = co_await when_all(task1(), task2(), task3());
        auto endTime = std::chrono::steady_clock::now();
        double duration_second = std::chrono::duration<double>(endTime-startTime).count();
        std::cout << "Task4: �ȴ����, �ȴ���ʱ: " << duration_second << "s\n";

        std::cout << "task1 result: " << result1 << " " << "task2 result: " << result2 << "\n";
    };

    sync_wait(task4());

    return 0;
}

/*
 * @ result
    main�߳� 1
    Task4: Start, �ȴ���
            task4�߳� 1
    Task1: Start, sleep 5s
    task1�߳� 1
    Task2: Start, sleep 8s
    task2�߳� 1
    Task3: Start, after 6s set m_stop = true
    task3�߳� 1
    Task1: Resume after 5s
    Task3: Resume after 6s
    Task2: Resume after 8s
    Task4: �ȴ����, �ȴ���ʱ: 8.01385s
    task1 result: 1 task2 result: 2
*/