#ifndef CORO_IO_SCHEDULER_HPP
#define CORO_IO_SCHEDULER_HPP

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include <atomic>
#include <chrono>
#include <coroutine>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "coro/detail/poll_info.hpp"
#include "coro/poll.hpp"
#include "coro/task.hpp"
#include "coro/thread_pool.hpp"

#ifdef NETWORKING

#include "coro/net/socket.hpp"

#endif

using namespace coro::detail;
using namespace coro::net;

namespace coro {

    class IoScheduler {
    public:
        using timed_events = detail::PollInfo::timed_events;
        using time_point = std::chrono::steady_clock::time_point;
        using clock = std::chrono::steady_clock;

        enum class ExecutionStrategy {
            On_ThreadPool,
            On_ThreadInline,
        };

        struct Options {
            ExecutionStrategy execution_strategy{ExecutionStrategy::On_ThreadPool};
            std::size_t threads_count{std::thread::hardware_concurrency()};
        };

        // 公开接口
        static std::shared_ptr<IoScheduler> make_shared(Options opts = {
                                                            ExecutionStrategy::On_ThreadPool,
                                                            std::thread::hardware_concurrency()});

        IoScheduler(IoScheduler&&) = delete;
        auto operator=(IoScheduler&&) = delete;
        ~IoScheduler();

        void shutdown();
        bool spawn(Task<void>&& task);
        bool resume(std::coroutine_handle<> handle);
        std::size_t size() const noexcept;

        // 调度相关
        struct ScheduleAwaiter;
        ScheduleAwaiter schedule();
        Task<> schedule_after(std::chrono::milliseconds amount);
        Task<> schedule_at(std::chrono::steady_clock::time_point time);

        // 协程控制
        ScheduleAwaiter yield();
        Task<> yield_for(std::chrono::milliseconds amount);
        Task<> yield_until(std::chrono::steady_clock::time_point time);

        // I/O操作
        Task<PollStatus> poll(int fd, PollOp op, std::chrono::milliseconds timeout);

#ifdef NETWORKING
        Task<PollStatus> poll(net::Socket& sock, PollOp op,
                              std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
            return poll(sock.fd(), op, timeout);
        }
#endif

        // Awaiter结构
        struct ScheduleAwaiter {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> awaiting_handle) noexcept {
                if (m_scheduler.m_opts.execution_strategy == ExecutionStrategy::On_ThreadInline) {
                    m_scheduler.m_size.fetch_add(1, std::memory_order_release);
                    {
                        std::scoped_lock<std::mutex> lk{m_scheduler.m_tasks_mutex};
                        m_scheduler.m_tasks.emplace_back(awaiting_handle);
                    }

                    bool expected{false};
                    if (m_scheduler.m_schedule_fd_triggered.compare_exchange_strong(
                            expected, true, std::memory_order_release, std::memory_order_relaxed)) {
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

    private:
        // 私有实现
        IoScheduler(Options opts);
        void run();
        void on_timeout();
        void on_schedule();
        PollStatus event_to_poll_status(uint32_t events);

        // 定时器管理
        timed_events::iterator add_time_token(time_point tp, detail::PollInfo& pi);
        void remove_timer_token(timed_events::iterator pos);
        void update_timeout(time_point now);

        // 成员变量
        Options m_opts;
        int m_epoll_fd{-1};
        int m_shutdown_fd{-1};
        int m_timer_fd{-1};
        int m_schedule_fd{-1};

        std::thread m_io_thread;
        std::unique_ptr<ThreadPool> m_thread_pool{nullptr};

        std::atomic<bool> m_shutdown{false};
        std::atomic<bool> m_schedule_fd_triggered{false};
        std::atomic<std::size_t> m_size{0};

        std::vector<std::coroutine_handle<>> m_tasks;
        std::mutex m_tasks_mutex;

        timed_events m_timed_events;
        std::mutex m_timed_events_mutex;

        static constexpr int m_max_events = 128;
        std::array<struct epoll_event, m_max_events> m_events{};

        // 静态常量指针
        static const constexpr int m_shutdown_object{};
        static const constexpr void* m_shutdown_ptr = &m_shutdown_object;
        static const constexpr int m_timer_object{};
        static const constexpr void* m_timer_ptr = &m_timer_object;
        static const constexpr int m_schedule_object{};
        static const constexpr void* m_schedule_ptr = &m_schedule_object;
    };

    // 内联常量定义
    inline constexpr IoScheduler::ExecutionStrategy io_exec_thread_inline =
        IoScheduler::ExecutionStrategy::On_ThreadInline;
    inline constexpr IoScheduler::ExecutionStrategy io_exec_thread_pool =
        IoScheduler::ExecutionStrategy::On_ThreadPool;

}  // namespace coro

#endif  // CORO_IO_SCHEDULER_HPP