//
// Created by JYH on 2025-02-22.
//

#ifndef CORO_SYNC_WAIT_HPP
#define CORO_SYNC_WAIT_HPP

#include "awaitable.hpp"

#include <coroutine>
#include <utility>
#include <variant>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <type_traits>


template <typename T = void>
class SyncWaitTask;

class SyncWaitEvent {
public:
    SyncWaitEvent(bool set = false) : m_set(set) {}
    ~SyncWaitEvent() = default;
    SyncWaitEvent(SyncWaitEvent&&) = delete;

    void set() noexcept {
        m_set.exchange(true);
        std::unique_lock<std::mutex> lk {m_mutex};
        m_cv.notify_all();
    }

    void reset() noexcept {
        m_set.exchange(false);
    }

    void wait() {
        std::unique_lock<std::mutex> lk {m_mutex};
        m_cv.wait(lk, [this]() {
            return m_set.load();
        });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_set {false};
};

class SyncWaitSyncWaitPromiseBase {
public:
    SyncWaitSyncWaitPromiseBase() = default;
    ~SyncWaitSyncWaitPromiseBase() = default;

public:
    auto initial_suspend() noexcept { return std::suspend_always{}; }

protected:
    std::coroutine_handle<> m_handle;
    SyncWaitEvent* m_event;
};


template <typename T>
class SyncWaitPromise : public SyncWaitSyncWaitPromiseBase {
public:
    SyncWaitPromise() = default;
    SyncWaitPromise(SyncWaitPromise&&) = delete;
    ~SyncWaitPromise() = default;

public:
    using variant_type = std::variant<T, std::exception_ptr>;

    void start(SyncWaitEvent& event) {
        m_event = &event;
        std::coroutine_handle<SyncWaitPromise<T>>::from_promise(*this).resume();
    }

    auto get_return_object() noexcept;
    //auto get_return_object() noexcept { return SyncWaitTask<T> { std::coroutine_handle<SyncWaitPromise>::from_promise(*this) }; }

    auto final_suspend() noexcept {
        struct CompleteNotifier {
            auto await_ready() noexcept {return false; }
            auto await_suspend(std::coroutine_handle<SyncWaitPromise<T>> h) noexcept {
                h.promise().m_event->set();
            }
            auto await_resume() noexcept {}
        };

        return CompleteNotifier{};
    }

    auto unhandled_exception() { new(&m_storage) variant_type(std::current_exception()); }

    auto return_value(T value) { m_storage.template emplace<T>(value); }

    T result() { return std::get<T>(m_storage); }

private:
    variant_type m_storage{};
};


template <>
class SyncWaitPromise<void> : public SyncWaitSyncWaitPromiseBase {
public:
    SyncWaitPromise() = default;
    SyncWaitPromise(SyncWaitPromise&&) = delete;
    ~SyncWaitPromise() = default;

public:
    void start(SyncWaitEvent& event) {
        m_event = &event;
        std::coroutine_handle<SyncWaitPromise<void>>::from_promise(*this).resume();
    }

    auto get_return_object() noexcept;
    //auto get_return_object() noexcept { return SyncWaitTask<void> { std::coroutine_handle<SyncWaitPromise>::from_promise(*this) }; }

    auto final_suspend() noexcept {
        struct CompleteNotifier {
            auto await_ready() noexcept {return false; }
            auto await_suspend(std::coroutine_handle<SyncWaitPromise<void>> h) noexcept {
                h.promise().m_event->set();
            }
            auto await_resume()  noexcept {}
        };

        return CompleteNotifier{};
    }

    void unhandled_exception() { m_exception_ptr = std::current_exception(); }

    void return_void() {}

    void result() {
        if (m_exception_ptr) {
            std::rethrow_exception(m_exception_ptr);
        }
    }

private:
    std::exception_ptr m_exception_ptr;
};


template <typename T>
class SyncWaitTask {
public:
    using promise_type = SyncWaitPromise<T>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

public:
    SyncWaitTask() noexcept : m_coroutine(nullptr) {}
    explicit SyncWaitTask(coroutine_handle handle) noexcept : m_coroutine(handle) {}

    SyncWaitTask(const SyncWaitTask&) = delete;
    SyncWaitTask& operator=(const SyncWaitTask&) = delete;

    SyncWaitTask(SyncWaitTask&& other) noexcept {
        m_coroutine = std::exchange(other.m_coroutine, nullptr);
    }
    SyncWaitTask& operator=(SyncWaitTask&& other) noexcept {
        if (std::addressof(other) != this) {
            if (m_coroutine != nullptr) {
                m_coroutine.destroy();
            }
            m_coroutine = std::exchange(other.m_coroutine, nullptr);
        }
        return *this;
    }

    ~SyncWaitTask() {
        if (m_coroutine != nullptr) {
            m_coroutine.destroy();
        }
    }

public:
    promise_type& promise() & { return m_coroutine.promise(); }
    promise_type&& promise() && { return std::move(m_coroutine.promise()); }

private:
    coroutine_handle m_coroutine;
};


template <typename T>
inline auto SyncWaitPromise<T>::get_return_object() noexcept {
    return SyncWaitTask<T> { std::coroutine_handle<SyncWaitPromise>::from_promise(*this) };
}


inline auto SyncWaitPromise<void>::get_return_object() noexcept {
    return SyncWaitTask<void> { std::coroutine_handle<SyncWaitPromise>::from_promise(*this) };
}

template  <Awaitable A, typename T = typename AwaitableTraits<A>::ReturnType>
    static auto make_sync_wait_task(A&& awaitable) -> SyncWaitTask<T> {
    if constexpr (std::is_void_v<T>) {
        co_await std::forward<A>(awaitable);
        co_return;
    } else {
        co_return co_await std::forward<A>(awaitable);
    }
}

template <Awaitable A, typename T = typename AwaitableTraits<A>::ReturnType>
    auto sync_wait(A&& awaitable) -> decltype(auto) {
    SyncWaitEvent e{};
    auto task = make_sync_wait_task(std::forward<A>(awaitable));
    task.promise().start(e);
    e.wait();

    if constexpr (std::is_void_v<T>) {
        task.promise().result();
        return;
    } else if constexpr (std::is_move_assignable_v<T>) {
        auto result = std::move(task).promise().result();
        return result;
    } else {
        return task.promise().result();
    }
}

#endif //CORO_SYNC_WAIT_HPP
