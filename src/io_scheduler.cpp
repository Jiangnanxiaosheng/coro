#include "coro/io_scheduler.hpp"
#include <atomic>

namespace coro {

    IoScheduler::IoScheduler(Options opts) : m_opts(opts), m_epoll_fd(epoll_create1(EPOLL_CLOEXEC)),
                                             m_shutdown_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)),
                                             m_schedule_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)),
                                             m_timer_fd(timerfd_create(CLOCK_MONOTONIC,  TFD_CLOEXEC | TFD_NONBLOCK)) {}

    std::shared_ptr<IoScheduler> IoScheduler::make_shared(Options opts) {
        auto s = std::shared_ptr<IoScheduler>(new IoScheduler(opts));
        if (opts.execution_strategy == ExecutionStrategy::On_ThreadPool) {
            s->m_thread_pool = std::make_unique<ThreadPool>(s->m_opts.threads_count);
        }

        epoll_event e{};
        e.events = EPOLLIN;

        e.data.ptr = const_cast<void*>(m_shutdown_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_shutdown_fd, &e);

        e.data.ptr = const_cast<void*>(m_timer_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_timer_fd, &e);

        e.data.ptr = const_cast<void*>(m_schedule_ptr);
        epoll_ctl(s->m_epoll_fd, EPOLL_CTL_ADD, s->m_schedule_fd, &e);

        s->m_io_thread = std::thread([s]() { s->run(); });

        return s;
    }

    IoScheduler::~IoScheduler() {
        shutdown();

        if (m_io_thread.joinable()) {
            m_io_thread.join();
        }

        if (m_epoll_fd != -1) {
            close(m_epoll_fd);
            m_epoll_fd = -1;
        }
        if (m_schedule_fd != -1) {
            close(m_shutdown_fd);
            m_shutdown_fd = -1;
        }
        if (m_timer_fd != -1) {
            close(m_timer_fd);
            m_timer_fd = -1;
        }
    }

    Task<PollStatus> IoScheduler::poll(int fd, PollOp op, std::chrono::microseconds timeout) {
        m_size.fetch_add(1, std::memory_order_release);

        detail::PollInfo pi;
        pi.m_fd = fd;

        epoll_event e;
        e.events = static_cast<int>(op) | EPOLLONESHOT | EPOLLRDHUP;
        e.data.ptr = &pi;
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &e) == -1) {
            std::cerr << "epoll ctl error on fd " << fd << "\n";
        }

        auto result = co_await pi;
        m_size.fetch_sub(1, std::memory_order_release);

        co_return result;
    }

    bool IoScheduler::spawn(Task<void>&& task) {
        m_size.fetch_add(1, std::memory_order_release);
        auto owned_task = detail::make_self_deleting_task(std::move(task));
        owned_task.promise().executor_size(m_size);
        return resume(owned_task.handle());
    }

    bool IoScheduler::resume(std::coroutine_handle<> handle) {
        if (handle == nullptr || handle.done()) {
            return false;
        }

        if (m_stopping.load(std::memory_order::acquire)) {
            return false;
        }

        if (m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
            m_size.fetch_add(1, std::memory_order::release);
            {
                std::scoped_lock<std::mutex> lk{m_tasks_mutex};
                m_tasks.emplace_back(handle);
            }

            bool expected{false};
            if (m_has_been_scheduled.compare_exchange_strong(
                    expected, true, std::memory_order::release, std::memory_order::relaxed)) {
                eventfd_t value{1};
                eventfd_write(m_schedule_fd, value);
            }
            return true;
        } else {
            return m_thread_pool->resume(handle);
        }
    }

    void IoScheduler::shutdown() noexcept {
        if (!m_stopping.exchange(true, std::memory_order_acq_rel)) {
            if (m_thread_pool != nullptr) {
                m_thread_pool->shutdown();
            }
        }

        eventfd_t val{1};
        eventfd_write(m_shutdown_fd, val);
    }

    void IoScheduler::run() {
        while (!m_stopping.load(std::memory_order_acquire) || size() > 0) {
            int ifds = epoll_wait(m_epoll_fd, m_events.data(), m_max_events, -1);
            for (int i = 0; i < ifds; ++i) {
                epoll_event& event = m_events[i];
                auto* handle_ptr = event.data.ptr;

                // 处理调度事件（只有当 execution_strategy = ExecutionStrategy::On_ThreadInline 才会进入此分支）
                if (handle_ptr == m_schedule_ptr) {


                // 处理定时事件
                } else if (handle_ptr == m_timer_ptr) {


                // 处理关闭事件
                } else if (handle_ptr == m_shutdown_ptr) [[unlikely]] {
                    eventfd_t val;
                    eventfd_read(m_shutdown_fd, &val);

                // 处理io事件
                } else {
                    auto* pi = static_cast<detail::PollInfo*>(handle_ptr);
                    if (!pi->m_processed.exchange(true)) {
                        if (pi->m_fd != -1) {
                            epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, pi->m_fd, nullptr);
                        }


                        pi->m_poll_status = event_to_poll_status(event.events);

                        m_tasks.emplace_back(pi->m_awaiting_handle);
                    }
                }

            }

            if (!m_tasks.empty()) {
                std::vector<std::coroutine_handle<>> tasks{};
                {
                    std::scoped_lock<std::mutex> lk{m_tasks_mutex};
                    tasks.swap(m_tasks);
                }

                if (m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
                    for (auto& task : tasks) {
                        task.resume();
                    }
                } else {
                    for (auto& task : tasks) {
                        m_thread_pool->resume(task);
                    }
                }
            }
        }
    }

    PollStatus IoScheduler::event_to_poll_status(uint32_t events) {
        if (events & EPOLLIN || events & EPOLLOUT) {
            return PollStatus::Event;
        } else if (events & EPOLLERR) {
            return PollStatus::Error;
        } else if (events & EPOLLRDHUP || events & EPOLLHUP) {
            return PollStatus::Closed;
        }

        throw std::runtime_error{"invalid epoll state"};
    }

    std::size_t IoScheduler::size() const {
        if (m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
            return m_size.load(std::memory_order::acquire);
        } else {
            return m_size.load(std::memory_order::acquire) + m_thread_pool->size();
        }
    }






} // namespace coro