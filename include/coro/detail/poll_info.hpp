#ifndef CORO_POLL_INFO_HPP
#define CORO_POLL_INFO_HPP

#include <chrono>
#include <map>
#include <optional>
#include <atomic>
#include <coroutine>

#include "coro/poll.hpp"

namespace coro::detail {

    // 等待 poll 事件时保存的详细信息，类似于reactor模型中channel的作用
    struct PollInfo {
        using clock = std::chrono::steady_clock;
        using time_point = clock::time_point;
        using timed_events = std::multimap<time_point, PollInfo*>;

        PollInfo() = default;
        auto operator=(PollInfo&&) = delete;
        ~PollInfo() = default;

        struct PollAwaiter {
            PollAwaiter(PollInfo& pi) : pi(pi) {}

            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) noexcept {
                pi.m_awaiting_handle = h;
                std::atomic_thread_fence(std::memory_order::release);
            }
            auto await_resume() noexcept { return pi.m_poll_status; }

            PollInfo& pi;
        };

        auto operator co_await() { return PollAwaiter{*this}; }

        int m_fd{-1};   // poll operation 对应的 fd
        std::optional<timed_events::iterator> m_timer_pos {std::nullopt};  // 记录定时事件在multi_map中的位置
        PollStatus m_poll_status {PollStatus::Error};  // poll operation 完成后返回的状态
        std::coroutine_handle<> m_awaiting_handle;    // 记录 co_await pi 时所在的协程，当poll operation 返回时恢复到之前的协程
        std::atomic<bool> m_processed{false};     // poll operation 是否被处理，事件本身和定时事件（如果该事件超时）只能处理一次
    };

} // namespace coro::detail

#endif