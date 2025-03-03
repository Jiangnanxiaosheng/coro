//
// Created by yh on 3/3/25.
//

#ifndef CORO_IO_SCHEDULER_HPP
#define CORO_IO_SCHEDULER_HPP

#include <iostream>
#include <coroutine>
#include <utility>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include "poll.hpp"
#include "socket.hpp"
#include "poll_info.hpp"

#include "../task.hpp"
#include "../threadpool.hpp"

using namespace std::chrono_literals;

class IoScheduler {
public:
    using timed_events = PollInfo::timed_events;
    using time_point = std::chrono::steady_clock::time_point;
    using clock = std::chrono::steady_clock;

    enum class ExecutionStrategy {
        On_ThreadPool, On_ThreadInline,
    };
    struct Options {
        ExecutionStrategy execution_strategy {ExecutionStrategy::On_ThreadPool};
        std::size_t threads_count {std::thread::hardware_concurrency()};
    };

private:
    IoScheduler(Options opts) : m_opts(opts), m_epoll_fd(epoll_create1(EPOLL_CLOEXEC)),
                                m_shutdown_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)),
                                m_timer_fd(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
                                m_schedule_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {}

public:
    static std::shared_ptr<IoScheduler> make_shared(Options opts) {
        std::shared_ptr<IoScheduler> s = std::shared_ptr<IoScheduler>(new IoScheduler(std::move(opts)));
        if (s->m_opts.execution_strategy == ExecutionStrategy::On_ThreadPool) {
            s->m_thread_pool = std::make_unique<ThreadPool>(s->m_opts.threads_count);
        }

        struct epoll_event e{};
        e.events = EPOLLIN;

        e.data.ptr = const_cast<void*>(m_shutdown_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_shutdown_fd, &e);

        e.data.ptr = const_cast<void*>(m_timer_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_timer_fd, &e);

        e.data.ptr = const_cast<void*>(m_schedule_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_schedule_fd, &e);

        s->m_io_thread = std::thread([s] { s->run(); });

        return s;
    }

    IoScheduler(IoScheduler&&) = delete;
    auto operator=(IoScheduler&&) = delete;

    ~IoScheduler() {
        shutdown();

        if (m_io_thread.joinable()) {
            m_io_thread.join();
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

public:
    void shutdown() {
        if (!m_shutdown.exchange(true, std::memory_order_acq_rel)) {
            uint64_t value{1};
            eventfd_write(m_shutdown_fd, value);
        }
    }

    void run() {
        while (!m_shutdown.load(std::memory_order_acquire) || size() > 0) {
            auto event_count = epoll_wait(m_epoll_fd, m_events.data(), m_max_events, m_default_timeout.count());
            if (event_count > 0) {
                for (int i = 0; i < event_count; ++i) {
                    epoll_event& event = m_events[i];
                    void* handle_ptr = event.data.ptr;

                    if (handle_ptr == m_timer_ptr) {
                        on_timeout();
                    } else if (handle_ptr == m_schedule_ptr) {
                        on_schedule();
                    } else if (handle_ptr == m_shutdown_ptr) [[unlikely]] {
                        eventfd_t val{0};
                        eventfd_read(m_shutdown_fd, &val);

                        // 处理io事件
                    } else {
                        auto* pi = static_cast<PollInfo*>(handle_ptr);
                        if (!pi->m_processed) {
                            std::atomic_thread_fence(std::memory_order::acquire);
                            // 事件和超时可能发生在同一次epoll_wait调用中，确保只有一个被处理
                            pi->m_processed = true;

                            // 这个事件已经响应，删除这个fd，以便下次重复使用
                            if (pi->m_fd != -1) {
                                epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, pi->m_fd, nullptr);
                            }

                            // io事件被触发了，删除定时事件
                            if (pi->m_timer_pos.has_value()) {
                                remove_timer_token(pi->m_timer_pos.value());
                            }

                            pi->m_poll_status = event_to_poll_status(event.events);

                            while (pi->m_awaiting_handle == nullptr) {
                                std::atomic_thread_fence(std::memory_order::acquire);
                            }

                            m_tasks.emplace_back(pi->m_awaiting_handle);
                        }
                    }
                }
            }

            if (!m_tasks.empty()) {
                if (m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
                    for (auto& handle : m_tasks) {
                        handle.resume();
                    }
                } else {
                    // m_thread_pool->resume(m_tasks);
                    for (auto& handle : m_tasks) {
                        m_thread_pool->resume(handle);
                    }
                }
                m_tasks.clear();
            }
        }
    }

    PollStatus event_to_poll_status(uint32_t events) {
        if (events & EPOLLIN || events & EPOLLOUT) {
            return PollStatus::Event;
        } else if (events & EPOLLERR) {
            return PollStatus::Error;
        } else if (events & EPOLLRDHUP || events & EPOLLHUP) {
            return PollStatus::Closed;
        }
        throw std::runtime_error {"event_to_poll_status: unknown PollStatus"};
    }

    struct ScheduleAwaiter {
        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> awaiting_handle) noexcept {
            if (m_scheduler.m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
                m_scheduler.m_size.fetch_add(1, std::memory_order_release);
                {
                    std::scoped_lock lk {m_scheduler.m_tasks_mutex};
                    m_scheduler.m_tasks.emplace_back(awaiting_handle);
                }

                bool expected{false};
                if (m_scheduler.m_schedule_fd_triggered.compare_exchange_strong(expected, true, std::memory_order_release, std::memory_order_relaxed)) {
                    eventfd_t value{1};
                    // 触发调度事件
                    eventfd_write(m_scheduler.m_schedule_fd, value);
                }
            } else {
                m_scheduler.m_thread_pool->resume(awaiting_handle);
            }
        }
        void await_resume() noexcept {}

        IoScheduler& m_scheduler;
    };

    ScheduleAwaiter schedule () { return ScheduleAwaiter{*this}; }
    Task<> schedule_after(std::chrono::milliseconds amount) {
        if (amount <= 0ms) {
            co_await schedule();
        } else {
            m_size.fetch_add(1, std::memory_order_release);
            PollInfo pi{};
            // 设置定时器, 在 amount 毫秒后触发定时事件
            add_time_token(clock::now() + amount, pi);
            // 挂起当前协程，等待恢复
            co_await pi;
            m_size.fetch_sub(1, std::memory_order_release);
        }
        co_return;
    }
    Task<> schedule_at(std::chrono::steady_clock::time_point time) {
        auto now = clock::now();
        if (time <= now) {
            co_await schedule();
        } else {
            m_size.fetch_add(1, std::memory_order_release);
            auto amount = std::chrono::duration_cast<std::chrono::milliseconds>(time - now);

            PollInfo pi{};
            add_time_token(now + amount, pi);
            co_await pi;
            m_size.fetch_sub(1, std::memory_order_release);
        }
        co_return;
    }

    ScheduleAwaiter yield() { return schedule(); }
    Task<> yield_for(std::chrono::milliseconds amount) { return schedule_after(amount); }
    Task<> yield_until(std::chrono::steady_clock::time_point time) { return schedule_at(time); }

    Task<PollStatus> poll(int fd, PollOp op, std::chrono::milliseconds timeout) {
        m_size.fetch_add(1, std::memory_order_release);

        bool timeout_requested = (timeout > 0ms);

        PollInfo pi{};
        pi.m_fd = fd;

        if (timeout_requested) {
            pi.m_timer_pos = add_time_token(clock::now() + timeout, pi);
        }

        epoll_event e{};
        // EPOLLONESHOT: 只触发一次
        // EPOLLRDHUP: 对端关闭连接
        e.events = static_cast<int>(op) | EPOLLONESHOT | EPOLLRDHUP;
        e.data.ptr = &pi;
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &e) == -1) {
            std::cerr << "epoll ctl error on fd " << fd << "\n";
        }

        auto result = co_await pi;
        m_size.fetch_sub(1, std::memory_order_release);

        co_return result;
    }

    Task<PollStatus> poll(Socket& sock, PollOp op, std::chrono::milliseconds  timeout) { return poll(sock.fd(), op, timeout); }


    void on_timeout() {
        std::vector<PollInfo*> poll_infos {};
        auto now = clock::now();
        {
            std::scoped_lock lk {m_timed_events_mutex};
            while (!m_timed_events.empty()) {
                auto first    = m_timed_events.begin();
                auto [tp, pi] = *first;

                // 把超时的事件都加入 poll_infos
                if (tp <= now) {
                    m_timed_events.erase(first);
                    poll_infos.emplace_back(pi);
                } else {
                    break;
                }
            }
        }

        for (auto pi : poll_infos) {
            if (!pi->m_processed) {
                // 事件和超时可能发生在同一次epoll_wait调用中，确保只有一个被处理
                pi->m_processed = true;

                // 删除监听的io事件
                if (pi->m_fd != -1) {
                    epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, pi->m_fd, nullptr);
                }

                while (pi->m_awaiting_handle == nullptr) {
                    std::atomic_thread_fence(std::memory_order::acquire);
                    // std::cerr << "process_event_execute() has a nullptr event\n";
                }

                m_tasks.emplace_back(pi->m_awaiting_handle);
                // 设置这些事件的 PollStatus 为 Timeout
                pi->m_poll_status = PollStatus::Timeout;
            }
        }

        update_timeout(clock::now());
    }

    void on_schedule() {
        std::vector<std::coroutine_handle<>> tasks{};
        {
            std::scoped_lock lk{m_tasks_mutex};
            tasks.swap(m_tasks);

            eventfd_t value{0};
            // 清空 m_schedule_fd 的值
            eventfd_read(m_schedule_fd, &value);

            // 已经处理了schedule事件，将触发标志改为false
            m_schedule_fd_triggered.exchange(false, std::memory_order::release);
        }

        for (auto& task : tasks) {
            task.resume();
        }
        m_size.fetch_sub(tasks.size(), std::memory_order::release);
    }

    std::size_t size() noexcept {
        if (m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline)
        {
            return m_size.load(std::memory_order::acquire);
        } else {
            return m_size.load(std::memory_order::acquire) + m_thread_pool->size();
        }
    }

private:
    Options m_opts;

    int m_epoll_fd {-1};
    int m_shutdown_fd {-1};
    int m_timer_fd {-1};

    std::thread m_io_thread;
    std::unique_ptr<ThreadPool> m_thread_pool{nullptr};

    std::atomic<bool> m_shutdown {false};

    // 如果任务是在m_io_thread上运行的，则由原子变量判断是否已经 eventfd_write(m_schedule_fd)
    int m_schedule_fd {-1};
    std::atomic<bool> m_schedule_fd_triggered {false};

    // 等待恢复的协程任务
    std::vector<std::coroutine_handle<>> m_tasks;
    std::mutex m_tasks_mutex;

    // IoScheduler中 执行 + 等待 的任务数
    std::atomic<std::size_t> m_size{0};

    // 延迟调度(yield) 或 执行带超时时间poll操作的事件
    timed_events m_timed_events;
    std::mutex m_timed_events_mutex;

    static const constexpr int m_shutdown_object {};
    static const constexpr void* m_shutdown_ptr = &m_shutdown_object;

    static const constexpr int m_timer_object {};
    static const constexpr void* m_timer_ptr = &m_timer_object;

    static const constexpr int m_schedule_object {};
    static const constexpr void* m_schedule_ptr = &m_schedule_object;

    static constexpr int m_max_events = 128;
    // 存放epoll_wait() 返回事件的 array
    std::array<struct epoll_event, m_max_events> m_events {};

    static constexpr std::chrono::milliseconds m_default_timeout{1000};

    timed_events::iterator add_time_token(time_point tp, PollInfo& pi) {
        std::scoped_lock lk{m_timed_events_mutex};
        auto pos = m_timed_events.emplace(tp, &pi);

        // 如果插入的时间点是最早的，则更新 timerfd 触发时间
        if (pos == m_timed_events.begin()) {
            update_timeout(clock::now());
        }

        return pos;
    }

    void remove_timer_token(timed_events::iterator pos) {
        {
            std::scoped_lock lk {m_timed_events_mutex};
            // 检查是否是最早的任务
            auto is_first = (m_timed_events.begin() == pos);

            m_timed_events.erase(pos);

            // 如果被删除的是最早的任务，则需要更新 timerfd
            if (is_first) {
                update_timeout(clock::now());
            }
        }
    }

    void update_timeout(time_point now) {
        if (!m_timed_events.empty()) {
            // 获取最早的任务
            auto& [tp, pi] = *m_timed_events.begin();
            // 计算距离现在的时间间隔
            auto amount = tp - now;

            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(amount);
            amount -= seconds;
            auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(amount);

            if (seconds <= 0s) {
                seconds = 0s;
                if (nanoseconds <= 0ns) {
                    // 设置为1纳秒，确保定时器不会被禁用,而是立即触发
                    nanoseconds = 1ns;
                }
            }

            itimerspec ts{};
            ts.it_value.tv_sec = seconds.count();
            ts.it_value.tv_nsec = nanoseconds.count();
            // 更新 timerfd 的定时器时间
            if (timerfd_settime(m_timer_fd, 0, &ts, nullptr) == -1) {
                std::cerr << "failed to set timerfd errorno=[" << std::string{strerror(errno)} << "].";
            }
        } else {
            // 如果没有定时任务，则禁用 timerfd
            itimerspec ts{};
            ts.it_value.tv_sec = 0;
            ts.it_value.tv_nsec = 0;
            if (timerfd_settime(m_timer_fd, 0, &ts, nullptr) == -1) {
                std::cerr << "failed to set timerfd errorno=[" << std::string{strerror(errno)} << "].";
            }
        }
    }
};

inline constexpr IoScheduler::ExecutionStrategy iosched_exec_inline = IoScheduler::ExecutionStrategy::On_ThreadInline;
inline constexpr IoScheduler::ExecutionStrategy iosched_exec_pool = IoScheduler::ExecutionStrategy::On_ThreadPool;

#endif //CORO_IO_SCHEDULER_HPP
