#ifndef CORO_THREAD_POOL_HPP
#define CORO_THREAD_POOL_HPP

#include <iostream>
#include "coro/task.hpp"
#include "coro/detail/self_deleting_task.hpp"

#include <coroutine>
#include <thread>
#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>

namespace coro {
    class ThreadPool {
    public:
        ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency());
        ~ThreadPool();
        ThreadPool&operator=(ThreadPool &&) = delete;

        class Awaiter {
        public:
            explicit Awaiter(ThreadPool &p) : pool(p) {}

            auto await_ready() noexcept { return false; }

            // 在协程内部调用co_await tp.schedule(), 此处会将当前协程，即 awaiting_coroutine 放入ThreadPool的工作队列中
            auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept {
                pool.scheduler_impl(awaiting_coroutine);
            }

            auto await_resume() noexcept {}

        private:
            ThreadPool &pool;
        };

    public:
        [[nodiscard]] Awaiter schedule();

        // 允许当前任务主动让出执行权，重新排队到末尾，以便其他任务有机会执行，有助于避免长任务独占线程
        [[nodiscard]] Awaiter yield() {
            return schedule();
        }

        void shutdown() noexcept;

        /**
         * todo 在线程池上恢复一个协程句柄
         * @return true if coroutine is resumed
         */
        bool resume(std::coroutine_handle<> handle);

        /**
         * todo 在线程池上调度任务，并返回另一个等待原任务完成的新任务（返回值与原任务相同）
         * @param task 在线程池上调度的任务
         * @return 等待原任务完成的新任务
         */
        template<typename T>
        [[nodiscard]] Task<T> schedule(Task<T> task) {
            co_await schedule();
            co_return co_await task;
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
        void executor();
        void scheduler_impl(std::coroutine_handle<> handle) noexcept;

        std::vector<std::thread> m_threads;
        std::queue<std::coroutine_handle<>> m_queue;
        std::mutex m_queue_mutex;
        std::atomic<bool> m_stop{false};
        std::condition_variable m_cv;

        // 任务队列中等待的任务数量 + 正在执行的任务。
        std::atomic<std::size_t> m_size{0};
    };
} // namespace coro


#endif //CORO_THREAD_POOL_HPP
