              #include "../include/coro/thread_pool.hpp"

namespace  coro {
    ThreadPool::ThreadPool(std::size_t thread_count) {
        m_threads.reserve(thread_count);
        for (int i = 0; i < thread_count; ++i) {
            m_threads.emplace_back([this]() {
                executor();
            });
        }
    }

    ThreadPool::~ThreadPool() { shutdown(); }


    auto ThreadPool::schedule() -> Awaiter {
        m_size.fetch_add(1, std::memory_order_release);
        if (!m_stop.load(std::memory_order_acquire)) {
            return Awaiter{*this};
        } else {
            m_size.fetch_sub(1, std::memory_order_release);
            throw std::runtime_error("coro::ThreadPool is shutting down, unable to schedule new tasks.");
        }
    }

    void ThreadPool::executor() {
        while (!m_stop.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk{m_queue_mutex};
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
            std::unique_lock<std::mutex> lk{m_queue_mutex};

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

    void ThreadPool::shutdown() noexcept {
        if (!m_stop.exchange(true, std::memory_order_acq_rel)) {
            m_cv.notify_all();

            for (auto &thread: m_threads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        }
    }

    bool ThreadPool::resume(std::coroutine_handle<> handle) {
        if (handle == nullptr || handle.done()) {
            return false;
        }
        m_size.fetch_add(1, std::memory_order_release);
        if (m_stop.load(std::memory_order_acquire)) {
            m_size.fetch_sub(1, std::memory_order_release);
            return false;
        }
        scheduler_impl(handle);
        return true;
    }

    void ThreadPool::scheduler_impl(std::coroutine_handle<> handle) noexcept {
        if (handle == nullptr || handle.done()) {
            return;
        }

        {
            std::scoped_lock lk{m_queue_mutex};
            m_queue.emplace(handle);
            m_cv.notify_one();
        }
    }
} // namespace coro
