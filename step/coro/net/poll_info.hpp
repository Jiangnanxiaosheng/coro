//
// Created by yh on 2/28/25.
//

#ifndef CORO_POLL_INFO_HPP
#define CORO_POLL_INFO_HPP

#include <coroutine>
#include <optional>
#include <map>
#include <chrono>
#include <atomic>
#include <sys/epoll.h>

#include "poll.hpp"

struct PollInfo {
    using time_point = std::chrono::steady_clock::time_point;
    using timed_events = std::multimap<time_point, PollInfo*>;

    PollInfo() = default;

    PollInfo(PollInfo&&) = delete;
    PollInfo& operator=(PollInfo&&) = delete;

    struct PollAwaiter {
        PollAwaiter(PollInfo& pi): m_pi(pi) {}

        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> awaiting_handle) noexcept {
            m_pi.m_awaiting_handle = awaiting_handle;
            std::atomic_thread_fence(std::memory_order::release);
        }
        PollStatus await_resume() noexcept { return m_pi.m_poll_status; }

        PollInfo& m_pi;
    };

    PollAwaiter operator co_await() {
        return PollAwaiter{*this};
    }

    int m_fd {-1};
    std::optional<timed_events::iterator> m_timer_pos {std::nullopt};
    std::coroutine_handle<> m_awaiting_handle;
    PollStatus m_poll_status{PollStatus::Error};
    bool m_processed{false};
};

#endif //CORO_POLL_INFO_HPP
