#include "coro/io_scheduler.hpp"

#include <iostream>
#include <system_error>

using namespace std::chrono_literals;

namespace coro {

    IoScheduler::IoScheduler(Options opts)
        : m_opts(opts),
          m_epoll_fd(epoll_create1(EPOLL_CLOEXEC)),
          m_shutdown_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)),
          m_timer_fd(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
          m_schedule_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {
        if (m_epoll_fd == -1 || m_shutdown_fd == -1 || m_timer_fd == -1 || m_schedule_fd == -1) {
            throw std::system_error(errno, std::system_category(),
                                    "Failed to create scheduler fds");
        }
    }

    std::shared_ptr<IoScheduler> IoScheduler::make_shared(Options opts) {
        std::shared_ptr<IoScheduler> s =
            std::shared_ptr<IoScheduler>(new IoScheduler(std::move(opts)));

        if (opts.execution_strategy == ExecutionStrategy::On_ThreadPool) {
            s->m_thread_pool = std::make_unique<ThreadPool>(s->m_opts.threads_count);
        }

        struct epoll_event e{};
        e.events = EPOLLIN;

        // 添加监控文件描述符到epoll
        e.data.ptr = const_cast<void*>(m_shutdown_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_shutdown_fd, &e);

        e.data.ptr = const_cast<void*>(m_timer_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_timer_fd, &e);

        e.data.ptr = const_cast<void*>(m_schedule_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_schedule_fd, &e);

        s->m_io_thread = std::thread([s] { s->run(); });

        return s;
    }

    IoScheduler::~IoScheduler() {
        shutdown();

        if (m_io_thread.joinable()) {
            m_io_thread.join();
        }

        // 清理资源
        if (m_epoll_fd != -1)
            close(m_epoll_fd);
        if (m_timer_fd != -1)
            close(m_timer_fd);
        if (m_schedule_fd != -1)
            close(m_schedule_fd);
        if (m_shutdown_fd != -1)
            close(m_shutdown_fd);
    }

    void IoScheduler::shutdown() {
        if (!m_shutdown.exchange(true, std::memory_order_acq_rel)) {
            if (m_thread_pool) {
                m_thread_pool->shutdown();
            }
            uint64_t value{1};
            eventfd_write(m_shutdown_fd, value);
        }
    }

    void IoScheduler::run() {
        while (!m_shutdown.load(std::memory_order_acquire) || size() > 0) {
            auto event_count = epoll_wait(m_epoll_fd, m_events.data(), m_max_events, -1);

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
                        auto* pi = static_cast<detail::PollInfo*>(handle_ptr);
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
            //            if (!m_tasks.empty()) {
            //                if (m_opts.execution_strategy ==
            //                ExecutionStrategy::On_ThreadInline) {
            //                    for (auto& handle : m_tasks) {
            //                        handle.resume();
            //                    }
            //                } else {
            //                    // m_thread_pool->resume(m_tasks);
            //                    for (auto& handle : m_tasks) {
            //                        m_thread_pool->resume(handle);
            //                    }
            //                }
            //
            //                m_tasks.clear();
            //                std::cout << "清空了m_tasks()" << strNow() << "\n";
            //            }
            if (!m_tasks.empty()) {
                std::vector<std::coroutine_handle<>> tasks{};
                {
                    std::scoped_lock<std::mutex> lk{m_tasks_mutex};
                    tasks.swap(m_tasks);
                }
                if (m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
                    for (auto& handle : tasks) {
                        handle.resume();
                    }
                } else {
                    // m_thread_pool->resume(tasks);
                    for (auto& handle : tasks) {
                        m_thread_pool->resume(handle);
                    }
                }
            }
        }
    }

    bool IoScheduler::spawn(Task<void>&& task) {
        // std::cout << "spawn了\n";
        m_size.fetch_add(1, std::memory_order::release);
        // std::cout << "计数器+1\n";
        auto owned_task = coro::detail::make_self_deleting_task(std::move(task));
        owned_task.promise().executor_size(m_size);
        // std::cout << "即将恢复spawn(task)\n";
        return resume(owned_task.handle());
    }

    bool IoScheduler::resume(std::coroutine_handle<> handle) {
        // std::cout << "进入resume\n";
        if (handle == nullptr || handle.done()) {
            // std::cout << "handle==null\n";
            return false;
        }

        if (m_shutdown.load(std::memory_order::acquire)) {
            // std::cout << "m_shutdown==true\n";
            return false;
        }

        if (m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
            m_size.fetch_add(1, std::memory_order::release);
            {
                std::scoped_lock<std::mutex> lk{m_tasks_mutex};
                m_tasks.emplace_back(handle);
                // std::cout << "__m_tasks.emplace_back(handle);\n";
                // std::cout << "m_tasks.size(): " << m_tasks.size() << "\n";
            }

            bool expected{false};
            if (m_schedule_fd_triggered.compare_exchange_strong(
                    expected, true, std::memory_order::release, std::memory_order::relaxed)) {
                eventfd_t value{1};
                eventfd_write(m_schedule_fd, value);
            }
            // std::cout << "促发了调度任务" << strNow() << "\n";
            return true;
        } else {
            return m_thread_pool->resume(handle);
        }
    }

    std::size_t IoScheduler::size() const noexcept {
        if (m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
            return m_size.load(std::memory_order::acquire);
        } else {
            return m_size.load(std::memory_order::acquire) + m_thread_pool->size();
        }
    }

    void IoScheduler::on_timeout() {
        std::vector<detail::PollInfo*> poll_infos{};
        auto now = clock::now();
        {
            std::scoped_lock<std::mutex> lk{m_timed_events_mutex};
            while (!m_timed_events.empty()) {
                auto first = m_timed_events.begin();
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

    void IoScheduler::on_schedule() {
        std::vector<std::coroutine_handle<>> tasks{};
        {
            std::scoped_lock<std::mutex> lk{m_tasks_mutex};
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

    IoScheduler::ScheduleAwaiter IoScheduler::schedule() { return ScheduleAwaiter{*this}; }

    Task<> IoScheduler::schedule_after(std::chrono::milliseconds amount) {
        if (amount <= 0ms) {
            co_await schedule();
        } else {
            m_size.fetch_add(1, std::memory_order_release);
            detail::PollInfo pi{};
            // 设置定时器, 在 amount 毫秒后触发定时事件
            add_time_token(clock::now() + amount, pi);
            // 挂起当前协程，等待恢复
            co_await pi;
            m_size.fetch_sub(1, std::memory_order_release);
        }
        co_return;
    }
    Task<> IoScheduler::schedule_at(std::chrono::steady_clock::time_point time) {
        auto now = clock::now();
        if (time <= now) {
            co_await schedule();
        } else {
            m_size.fetch_add(1, std::memory_order_release);
            auto amount = std::chrono::duration_cast<std::chrono::milliseconds>(time - now);

            detail::PollInfo pi{};
            add_time_token(now + amount, pi);
            co_await pi;
            m_size.fetch_sub(1, std::memory_order_release);
        }
        co_return;
    }

    IoScheduler::ScheduleAwaiter IoScheduler::yield() { return schedule(); }

    Task<> IoScheduler::yield_for(std::chrono::milliseconds amount) {
        return schedule_after(amount);
    }
    Task<> IoScheduler::yield_until(std::chrono::steady_clock::time_point time) {
        return schedule_at(time);
    }

    Task<PollStatus> IoScheduler::poll(int fd, PollOp op, std::chrono::milliseconds timeout) {
        m_size.fetch_add(1, std::memory_order_release);

        bool timeout_requested = (timeout > 0ms);

        detail::PollInfo pi{};
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

    PollStatus IoScheduler::event_to_poll_status(uint32_t events) {
        if (events & EPOLLIN || events & EPOLLOUT) {
            return PollStatus::Event;
        } else if (events & EPOLLERR) {
            return PollStatus::Error;
        } else if (events & EPOLLRDHUP || events & EPOLLHUP) {
            return PollStatus::Closed;
        }
        throw std::runtime_error{"event_to_poll_status: unknown PollStatus"};
    }

    auto IoScheduler::add_time_token(time_point tp, detail::PollInfo& pi)
        -> timed_events::iterator {
        std::scoped_lock<std::mutex> lk{m_timed_events_mutex};
        auto pos = m_timed_events.emplace(tp, &pi);

        // 如果插入的时间点是最早的，则更新 timerfd 触发时间
        if (pos == m_timed_events.begin()) {
            update_timeout(clock::now());
        }

        return pos;
    }

    void IoScheduler::remove_timer_token(timed_events::iterator pos) {
        {
            std::scoped_lock<std::mutex> lk{m_timed_events_mutex};
            // 检查是否是最早的任务
            auto is_first = (m_timed_events.begin() == pos);

            m_timed_events.erase(pos);

            // 如果被删除的是最早的任务，则需要更新 timerfd
            if (is_first) {
                update_timeout(clock::now());
            }
        }
    }

    void IoScheduler::update_timeout(time_point now) {
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
                std::cerr << "failed to set timerfd errorno=[" << std::string{strerror(errno)}
                          << "].";
            }
        } else {
            // 如果没有定时任务，则禁用 timerfd
            itimerspec ts{};
            ts.it_value.tv_sec = 0;
            ts.it_value.tv_nsec = 0;
            if (timerfd_settime(m_timer_fd, 0, &ts, nullptr) == -1) {
                std::cerr << "failed to set timerfd errorno=[" << std::string{strerror(errno)}
                          << "].";
            }
        }
    }

}  // namespace coro