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
        // ��Э���ڲ�����co_await tp.schedule(), �˴��Ὣ��ǰЭ�̣��� awaiting_coroutine ����ThreadPool�Ĺ���������
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

    // ����ǰ���������ó�ִ��Ȩ�������Ŷӵ�ĩβ���Ա����������л���ִ�У������ڱ��ⳤ�����ռ�߳�
    Awaiter yield() {
        return schedule();
    }

    // �����̳߳صĴ�С
    std::size_t thread_count() const {
        return m_threads.size();
    }

    // ��������еȴ����������� + ����ִ�е�����
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

    // ��������еȴ����������� + ����ִ�е�����
    std::atomic<std::size_t> m_size {0};
};


int main() {

    ThreadPool pool(4);
    std::cout << "main���������߳�: " << std::this_thread::get_id() << "\n";

    auto task = [&pool]() -> Task<int> {
        co_await pool.schedule();

        std::cout << "Э������ʼ\n";
        std::cout << "Э�̺��������߳�: " << std::this_thread::get_id() << "\n";
        std::cout << "Э���������\n";
        co_return 199;
    };

    auto t = task();
    if (!t.done()) {
        t.resume();
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "main��������\n";
    return 0;

}