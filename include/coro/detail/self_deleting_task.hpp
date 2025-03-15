#ifndef CORO_SELF_DELETING_TASK_HPP
#define CORO_SELF_DELETING_TASK_HPP

#include "coro/task.hpp"

#include <coroutine>
#include <atomic>
#include <utility>

namespace coro::detail {

    class SelfDeletingTask;

    class SelfDeletingPromise {
    public:
        SelfDeletingPromise() = default;
        SelfDeletingPromise(SelfDeletingPromise&& other) noexcept
                : m_size_ptr(std::exchange(other.m_size_ptr, nullptr)) {}

        SelfDeletingPromise& operator=(SelfDeletingPromise&& other) noexcept {
            if (std::addressof(other) != this) {
                m_size_ptr = std::exchange(other.m_size_ptr, nullptr);
            }
            return *this;
        }

        ~SelfDeletingPromise() = default;

        SelfDeletingTask get_return_object();
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept {
            if (m_size_ptr != nullptr) {
                // 本任务执行完毕，任务数减一
                m_size_ptr->fetch_sub(1, std::memory_order_release);
            }

            return std::suspend_never{};
        }
        void return_void() {}
        void unhandled_exception() {}

        void executor_size(std::atomic<std::size_t>& executor_size) {
            m_size_ptr = &executor_size;
        }

    private:
        // 传入的任务数（在原调度中确保加上自身）
        std::atomic<std::size_t>* m_size_ptr {nullptr};
    };

    class SelfDeletingTask {
    public:
        using promise_type = SelfDeletingPromise;

        SelfDeletingTask(SelfDeletingPromise& promise) : m_promise(&promise) {}
        SelfDeletingTask(SelfDeletingTask&& other) noexcept : m_promise(other.m_promise) {}
        SelfDeletingTask& operator=(SelfDeletingTask&& other) noexcept {
            if (std::addressof(other) != this) {
                m_promise = other.m_promise;
            }
            return *this;
        }

        SelfDeletingPromise& promise() { return *m_promise; }
        auto handle() -> std::coroutine_handle<SelfDeletingPromise> {
            return std::coroutine_handle<SelfDeletingPromise>::from_promise(*m_promise);
        }

    private:
        SelfDeletingPromise* m_promise;
    };

    inline auto make_self_deleting_task(Task<void> task) -> SelfDeletingTask {
        co_await task;
        co_return;
    }

    inline SelfDeletingTask SelfDeletingPromise::get_return_object() { return SelfDeletingTask{*this}; }

} //namespace coro::detail

#endif //CORO_SELF_DELETING_TASK_HPP
