//
// Created by JYH on 2025-02-23.
//
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <coroutine>
#include <chrono>
#include <queue>
#include <tuple>
#include <condition_variable>

#include "coro/task.hpp"
#include "coro/sync_wait.hpp"

class WhenAllLatch {
public:
    WhenAllLatch(std::size_t count) : m_count(count + 1) {}

    auto is_ready() {
        return m_awaiting_handle != nullptr && m_awaiting_handle.done();
    }

    // 尝试等待，设置待处理协程句柄
    auto try_await(std::coroutine_handle<> awaiting_handle) {
        m_awaiting_handle = awaiting_handle;
        return m_count.fetch_sub(1, std::memory_order_acq_rel) > 1;
    }

    // 当一个任务完成时通知等待协程
    void notify_awaitable_completed() {
        if (m_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            m_awaiting_handle.resume();  // 如果所有任务都完成了，恢复等待协程
        }
    }

private:
    std::atomic<std::size_t> m_count;
    std::coroutine_handle<> m_awaiting_handle {nullptr}; // 等待协程句柄
};

template <typename T>
class WhenAllAwaitable;

template <>
class WhenAllAwaitable<std::tuple<>> {
public:
    constexpr WhenAllAwaitable() noexcept {};
    constexpr WhenAllAwaitable(std::tuple<>) noexcept {}

    constexpr auto await_ready() noexcept { return true; }
    auto await_suspend(std::coroutine_handle<>) noexcept {}
    auto await_resume() noexcept -> std::tuple<> { return {}; }
};

template <typename ...T>
class WhenAllAwaitable<std::tuple<T...>> {
public:
    WhenAllAwaitable(T&&... tasks) : m_latch(sizeof...(T)), m_tasks(std::move(tasks)...) {}
    WhenAllAwaitable(std::tuple<T...>&& tasks) : m_latch(sizeof...(T)), m_tasks(std::move(tasks)) {}

    auto operator co_await() {
        struct Awaiter {
            Awaiter(WhenAllAwaitable& awaitable) : m_awaitable(awaitable) {}

            auto await_ready() noexcept { return m_awaitable.is_ready(); }
            auto await_suspend(std::coroutine_handle<> handle) noexcept {
                return m_awaitable.try_await(handle);
            }
            // auto await_resume() noexcept -> std::tuple<T...>& { return m_awaitable.m_tasks; }
            auto await_resume() -> std::tuple<typename T::ResultType...> {
                return std::apply([](auto&&... tasks) {
                    return std::make_tuple(tasks.result()...);
                }, m_awaitable.m_tasks);
            }

            WhenAllAwaitable& m_awaitable;
        };

        return Awaiter{*this};
    }

private:
    // 判断是否已完成所有任务
    bool is_ready() noexcept {
        return m_latch.is_ready();
    }

    auto try_await(std::coroutine_handle<> awaiting_handle) noexcept {
        std::apply([this](auto&&... tasks) {
            ((tasks.start(m_latch)), ...);  // 启动所有任务
        }, m_tasks);
        return m_latch.try_await(awaiting_handle); // 将主协程句柄传入latch中
    }

    WhenAllLatch m_latch;
    std::tuple<T...> m_tasks;
};

template <typename T>
class WhenAllPromise {
    using coroutine_handle = std::coroutine_handle<WhenAllPromise<T>>;
public:
    WhenAllPromise() = default;

    auto get_return_object() { return coroutine_handle::from_promise(*this);}
    auto initial_suspend() { return std::suspend_always{}; }
    auto final_suspend() noexcept {
        struct CompleteNotifier {
            auto await_ready() noexcept { return false; }
            auto await_suspend(coroutine_handle h) noexcept {
                h.promise().m_latch->notify_awaitable_completed();  // 通知主协程任务完成
            }
            auto await_resume() noexcept {}
        };
        return CompleteNotifier{};
    }

    auto unhandled_exception() noexcept {
        m_exception_ptr = std::current_exception();
    }

    // 保存任务结果
    auto yield_value(T&& value) noexcept {
        m_return_value = value;
        return final_suspend();
    }

    auto start(WhenAllLatch& latch) noexcept {
        m_latch = &latch;
        coroutine_handle::from_promise(*this).resume();
    }

    auto result() {
        if (m_exception_ptr) {
            std::rethrow_exception(m_exception_ptr);
        }
        return std::move(*m_return_value);
    }

    void return_void() {}

private:
    WhenAllLatch* m_latch{nullptr};
    std::exception_ptr m_exception_ptr {nullptr};
    std::optional<T> m_return_value;

};


template <>
class WhenAllPromise<void> {
    using coroutine_handle = std::coroutine_handle<WhenAllPromise<void>>;
public:
    WhenAllPromise() = default;

    auto get_return_object() { return coroutine_handle::from_promise(*this);}
    auto initial_suspend() { return std::suspend_always{}; }
    auto final_suspend() noexcept {
        struct CompleteNotifier {
            auto await_ready() noexcept { return false; }
            auto await_suspend(coroutine_handle h) noexcept {
                h.promise().m_latch->notify_awaitable_completed();  // 通知主协程任务完成
            }
            auto await_resume() noexcept {}
        };
        return CompleteNotifier{};
    }

    auto unhandled_exception() noexcept {
        m_exception_ptr = std::current_exception();
    }

    auto start(WhenAllLatch& latch) noexcept {
        m_latch = &latch;
        coroutine_handle::from_promise(*this).resume();
    }

    void result()  {
        if (m_exception_ptr) {
            std::rethrow_exception(m_exception_ptr);
        }
    }

    void return_void() {}

private:
    WhenAllLatch* m_latch{nullptr};
    std::exception_ptr m_exception_ptr {nullptr};
};

template <typename T>
class WhenAllTask {
public:
    template<class U>
    friend class WhenAllAwaitable;

    using promise_type = WhenAllPromise<T>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

    using ResultType =  T;

    WhenAllTask(coroutine_handle handle) :m_coroutine(handle) {}

    WhenAllTask(WhenAllTask&& other) noexcept
            : m_coroutine(std::exchange(other.m_coroutine, coroutine_handle{})) {}

    WhenAllTask& operator=(WhenAllTask&&) = delete;

    ~WhenAllTask() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }

    auto result() -> decltype(auto) {
        if constexpr (std::is_void_v<T>) {
            m_coroutine.promise().result();
            return ResultType{};
        } else {
            return m_coroutine.promise().result();
        }
    }

private:
    auto start(WhenAllLatch& latch) {
        m_coroutine.promise().start(latch);
    }

    coroutine_handle m_coroutine;
};


template <Awaitable A, typename T = typename AwaitableTraits<A&&>::ReturnType>
static auto make_when_all_task(A a) -> WhenAllTask<T> {
    if constexpr (std::is_void_v<T>) {
        co_await static_cast<A&&>(a);
        co_return;
    } else {
        co_yield co_await static_cast<A&&>(a);
    }
}

template <Awaitable... A>
auto when_all(A... a) {
    return WhenAllAwaitable<std::tuple<WhenAllTask<typename AwaitableTraits<A>::ReturnType>...>>(
            std::make_tuple(make_when_all_task(std::move(a))...));
}

int main() {
    std::cout << "main线程 " << std::this_thread::get_id() << "\n";

    auto task1 = []() -> Task<int> {
        std::cout << "task1线程 " << std::this_thread::get_id() << "\n";
        co_return 1;
    };

    auto task2 = []() -> Task<std::string> {
        std::cout << "task2线程 " << std::this_thread::get_id() << "\n";
        co_return "this is string";
    };


    auto task3 = [=]() -> Task<int> {
        std::cout << "task3线程 " << std::this_thread::get_id() << " 等待中\n";
        auto [x, y] = co_await when_all(task1(), task2());
        std::cout << "task3: 等待完成\n";
        std::cout << "task1 result: " << x << " " << "task2 result: " << y << "\n";
        co_return 3;
    };

    auto result = sync_wait(task3());
    std::cout << "task3 result: " << result << "\n";
    return 0;
}