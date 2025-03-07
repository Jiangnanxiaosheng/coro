//
// Created by yh on 3/3/25.
//

#ifndef CORO_TASK_SELF_DELETEING_HPP
#define CORO_TASK_SELF_DELETEING_HPP


#include "task.hpp"

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <utility>

class task_self_deleting;

class promise_self_deleting {
    public:
        promise_self_deleting() {}
        ~promise_self_deleting() {}

        promise_self_deleting(const promise_self_deleting&) = delete;
        promise_self_deleting(promise_self_deleting&& other) : m_executor_size(std::exchange(other.m_executor_size, nullptr)) {}
        auto operator=(const promise_self_deleting&)  = delete;
        promise_self_deleting& operator=(promise_self_deleting&& other) {
            if (std::addressof(other) != nullptr) {
                m_executor_size = std::exchange(other.m_executor_size, nullptr);
            }

            return *this;
        }


        auto get_return_object() -> task_self_deleting;


        auto initial_suspend() -> std::suspend_always {return std::suspend_always{};}
        auto final_suspend() noexcept -> std::suspend_never {
            if (m_executor_size != nullptr) {
                m_executor_size->fetch_sub(1, std::memory_order::release);
            }

            return std::suspend_never{};
        }
        void return_void() noexcept {}
        void unhandled_exception()  {}

        auto executor_size(std::atomic<std::size_t>& executor_size) {
            m_executor_size = &executor_size;
        }

    private:

        std::atomic<std::size_t>* m_executor_size{nullptr};
    };

    class task_self_deleting
    {
    public:
        using promise_type = promise_self_deleting;

        explicit task_self_deleting(promise_self_deleting& promise) : m_promise(&promise) {}
        ~task_self_deleting() {}

        task_self_deleting(const task_self_deleting&) = delete;
        task_self_deleting(task_self_deleting&& other)  : m_promise(other.m_promise) {}
        auto operator=(const task_self_deleting&) = delete;
        task_self_deleting& operator=(task_self_deleting&& other) {
            if (std::addressof(other) != this) {
                m_promise = other.m_promise;
            }

            return *this;
        }

        auto promise() -> promise_self_deleting& { return *m_promise; }
        auto handle() -> std::coroutine_handle<promise_self_deleting> {
            return std::coroutine_handle<promise_self_deleting>::from_promise(*m_promise);
        }

    private:
        promise_self_deleting* m_promise{nullptr};
    };


 auto make_task_self_deleting(Task<void> user_task)  -> task_self_deleting {
    co_await user_task;
    co_return;
}

auto promise_self_deleting::get_return_object()  -> task_self_deleting {
    return  task_self_deleting{*this};
}


#endif //CORO_TASK_SELF_DELETEING_HPP
