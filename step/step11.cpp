//
// Created by yh on 2/27/25.
//

#include <iostream>
#include <coroutine>
#include <utility>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <map>
#include <chrono>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include "coro/task.hpp"
#include "coro/net/poll.hpp"
#include "coro/threadpool.hpp"
#include "coro/net/socket.hpp"

using namespace std::chrono_literals;

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

class IoScheduler {
public:
    using timed_events = PollInfo::timed_events;

    enum class ExecutionStrategy {
        On_ThreadPool, On_InlineThread,
    };

private:
    IoScheduler() : m_epoll_fd(epoll_create1(1)),
                    m_stop_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) ,
                    m_timer_fd(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
                    m_schedule_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {}
public:
    std::shared_ptr<IoScheduler> make_shared(ExecutionStrategy execution_strategy) {
        std::shared_ptr<IoScheduler> s = std::shared_ptr<IoScheduler>(new IoScheduler());
        if (execution_strategy == ExecutionStrategy::On_ThreadPool) {
            m_thread_pool = std::make_unique<ThreadPool>();
        }

        struct epoll_event e{};
        e.events = EPOLLIN;

        e.data.ptr = const_cast<void*>(m_stop_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_stop_fd, &e);

        e.data.ptr = const_cast<void*>(m_timer_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_timer_fd, &e);

        e.data.ptr = const_cast<void*>(m_schedule_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_schedule_fd, &e);

        return s;
    }

    ~IoScheduler() {
        shutdown();

        if (m_thread.joinable()) {
            m_thread.join();
        }

        if (m_epoll_fd != -1) {
            close(m_epoll_fd);
            m_epoll_fd = -1;
        }

        if (m_timer_fd != -1) {
            close(m_timer_fd);
            m_timer_fd = -1;
        }

        if (m_schedule_fd != -1) {
            close(m_schedule_fd);
            m_schedule_fd = -1;
        }
    }


    struct ScheduleAwaiter {
        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> awaiting_handle) noexcept {

        }
        void await_resume() noexcept {}
    };

    ScheduleAwaiter schedule () { return ScheduleAwaiter{}; }
    Task<> schedule_after(std::chrono::milliseconds amount) {
        if (amount <= 0ms) {
            co_await  schedule();
        } else {

        }
        co_return;
    }
    Task<> schedule_at(std::chrono::steady_clock::time_point time) {
        auto now = std::chrono::steady_clock::now();
        if (time < now) {
            co_await schedule();
        } else {

        }
        co_return;
    }

    ScheduleAwaiter yield() { return schedule(); }
    Task<> yield_for(std::chrono::milliseconds amount) { return schedule_after(amount); }
    Task<> yield_until(std::chrono::steady_clock::time_point time) { return schedule_at(time); }

    Task<PollStatus> poll(int fd, PollOp op, std::chrono::milliseconds  timeout) {}
    Task<PollStatus> poll(Socket& sock, PollOp op, std::chrono::milliseconds  timeout) { return poll(sock.fd(), op, timeout); }

    void shutdown() {

    }

private:
    int m_epoll_fd {-1};
    int m_stop_fd {-1};
    int m_timer_fd {-1};
    int m_schedule_fd {-1};

    std::thread m_thread;
    std::unique_ptr<ThreadPool> m_thread_pool;

    std::atomic<bool> m_stop {false};
    std::atomic<bool> m_scheduler_fd_triggered {false};

    std::vector<std::coroutine_handle<>> m_tasks;
    std::mutex m_tasks_mutex;

    // 等待 + 正在运行的任务数
    std::atomic<std::size_t> m_size{0};

    std::vector<struct epoll_event> m_events {};

    timed_events m_timed_events;
    std::mutex m_timed_events_mutex;

    const int m_stop_object {};
    const void* m_stop_ptr = &m_stop_object;

    const int m_timer_object {};
    const void* m_timer_ptr = &m_timer_object;

    const int m_schedule_object {};
    const void* m_schedule_ptr = &m_schedule_object;
};

int main() {

}