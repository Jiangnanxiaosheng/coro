#ifndef CORO_IO_SCHEDULER_HPP
#define CORO_IO_SCHEDULER_HPP

#include <optional>
#include <coroutine>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>
#include <thread>
#include <map>
#include <mutex>

#ifdef  NETWORKING
#include "coro/net/socket.hpp"
#endif

#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include "coro/detail/poll_info.hpp"
#include "coro/task.hpp"
#include "coro/poll.hpp"
#include "coro/thread_pool.hpp"

namespace coro {
    class IoScheduler {
    public:
        using clock = std::chrono::steady_clock;
        using time_point = clock::time_point;
        using timed_events = detail::PollInfo::timed_events;

        enum class ExecutionStrategy {
            On_ThreadPool, On_ThreadInline,
        };
        struct Options {
            ExecutionStrategy execution_strategy {ExecutionStrategy::On_ThreadPool};
            std::size_t threads_count {std::thread::hardware_concurrency()};
        };

        static std::shared_ptr<IoScheduler> make_shared(Options opts =
                {ExecutionStrategy::On_ThreadPool, std::thread::hardware_concurrency()});

        auto operator=(IoScheduler&&) = delete;
        ~IoScheduler();

    public:
        struct ScheduleAwaiter {
            IoScheduler& scheduler;
            ScheduleAwaiter(IoScheduler& io) : scheduler(io) {}

            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) noexcept {
                if (scheduler.m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
                    scheduler.m_size.fetch_add(1, std::memory_order_release);

                    {
                        std::scoped_lock<std::mutex> lk {scheduler.m_tasks_mutex};
                        scheduler.m_tasks.emplace_back(h);
                    }

                    bool expected{false};
                    if (scheduler.m_has_been_scheduled.compare_exchange_strong(expected, true, std::memory_order_release, std::memory_order_relaxed)) {
                        eventfd_t  value{1};
                        eventfd_write(scheduler.m_schedule_fd, value);
                    }

                } else {
                    scheduler.m_thread_pool->resume(h);
                }

            }
            void await_resume() noexcept {}
        };
        [[nodiscard]] auto schedule() { return ScheduleAwaiter{*this}; }
        [[nodiscard]] auto yield() { return schedule(); }

        // 调度一个协程在延迟后执行
        Task<void> schedule_after(std::chrono::milliseconds amount);
        // 安排协程在特定时间执行
        Task<void> schedule_at(time_point time);
        // 暂停协程，直到指定的持续时间过去
        Task<void> yield_for(std::chrono::milliseconds amount);
        // 暂停协程，直到达到指定时间
        Task<void> yield_until(time_point time);

        bool spawn(Task<void>&& task);
        bool resume(std::coroutine_handle<> handle);

        Task<PollStatus> poll(int fd, PollOp op, std::chrono::microseconds timeout = std::chrono::microseconds(0));

#ifdef NETWORKING
        Task<PollStatus> poll(const net::Socket& sock, PollOp op,
                              std::chrono::microseconds timeout = std::chrono::microseconds(0)) {
            return poll(sock.fd(), op, timeout);
        }
#endif
        void shutdown() noexcept;
        void run();
        PollStatus event_to_poll_status(uint32_t events);
        std::size_t size() const;



    private:
        explicit IoScheduler(Options opts);

        Options m_opts;
        int m_epoll_fd;
        int m_timer_fd;
        int m_shutdown_fd;
        int m_schedule_fd;
        std::atomic<bool> m_has_been_scheduled {false};

        std::thread m_io_thread;
        std::unique_ptr<ThreadPool> m_thread_pool {nullptr};

        std::atomic<bool> m_stopping;


        std::atomic<std::size_t> m_size;
        std::vector<std::coroutine_handle<>> m_tasks;
        std::mutex m_tasks_mutex;

        static constexpr int m_max_events {128};
        std::array<epoll_event, m_max_events>m_events {};

        static constexpr int m_timer_obj{1};
        static constexpr int m_shutdown_obj{1};
        static constexpr int m_schedule_obj{1};

        static constexpr const void* m_timer_ptr = &m_timer_obj;
        static constexpr const void* m_shutdown_ptr = &m_shutdown_obj;
        static constexpr const void* m_schedule_ptr = &m_schedule_obj;

    };

    inline constexpr IoScheduler::ExecutionStrategy io_exec_thread_inline = IoScheduler::ExecutionStrategy::On_ThreadInline;
    inline constexpr IoScheduler::ExecutionStrategy io_exec_thread_pool = IoScheduler::ExecutionStrategy::On_ThreadPool;

} // namespace coro


#endif //CORO_IO_SCHEDULER_HPP
