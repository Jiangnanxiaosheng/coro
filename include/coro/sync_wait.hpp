//
// Created by yh on 3/12/25.
//

#ifndef CORO_SYNC_WAIT_HPP
#define CORO_SYNC_WAIT_HPP

#include "coro/concepts/awaitable.hpp"
#include <variant>
#include <exception>
#include <coroutine>
#include <mutex>
#include <atomic>
#include <type_traits>
#include <utility>
#include <condition_variable>

namespace concepts = coro::concepts;

namespace coro::detail {

    class SyncWaitEvent {
    public:
        SyncWaitEvent(bool set = false);
        auto operator=(SyncWaitEvent&&) = delete;
        ~SyncWaitEvent() = default;

        void set() noexcept;
        void reset() noexcept;
        void wait() noexcept;

    private:
        std::mutex m_mutex;
        std::atomic<bool> m_set{false};
        std::condition_variable m_cv;
    };

    struct SyncWaitPromiseBase {

        SyncWaitPromiseBase() = default;

        virtual ~SyncWaitPromiseBase() = default;

        auto initial_suspend() { return std::suspend_always{}; }

    protected:
        std::coroutine_handle<> m_previousHandle;
        SyncWaitEvent *m_event{nullptr};
    };

    template<typename T>
    class SyncWaitPromise final : public SyncWaitPromiseBase {
        // 支持存储引用类型，如果T是引用，那么存储移除引用限定类型的指针，否则存储移除const限定类型
        using stored_type = std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T>*, std::remove_const_t<T>>;
        using variant_type = std::variant<std::monostate, stored_type, std::exception_ptr>;
        using coroutine_handle = std::coroutine_handle<SyncWaitPromise<T>>;

    public:
        SyncWaitPromise() = default;

        SyncWaitPromise &operator=(SyncWaitPromise &&) = delete; // 同时会删除拷贝构造、拷贝赋值、移动构造
        ~SyncWaitPromise() = default;

    public:

        void start(SyncWaitEvent &e) {
            m_event = &e;
            coroutine_handle::from_promise(*this).resume();
        }

        auto get_return_object() noexcept { return coroutine_handle::from_promise(*this); }

        void unhandled_exception() noexcept {
            m_storage.template emplace<std::exception_ptr>(std::current_exception());
        }

        auto final_suspend() noexcept {
            struct CompleteNotifier {
                auto await_ready() noexcept { return false; }

                /*
                 * Todo 设置coroutine完成事件已经触发
                 */
                void await_suspend(coroutine_handle h) noexcept {
                    h.promise().m_event->set();
                }

                auto await_resume() noexcept {}
            };
            return CompleteNotifier{};
        }

        template<typename ValueType>
        requires(std::is_reference_v<T> and std::is_constructible_v<T, ValueType &&>) or
                (not std::is_reference_v<T> and std::is_constructible_v<stored_type, ValueType &&>)

        void return_value(ValueType &&value) {
            if constexpr (std::is_reference_v<T>) {
                T ref = static_cast<ValueType &&>(value);
                m_storage.template emplace<stored_type>(std::addressof(ref));

            } else {
                m_storage.template emplace<stored_type>(std::forward<ValueType>(value));
            }
        }

        void return_value(stored_type &&value) requires(not std::is_reference_v < T >) {
            if constexpr (std::is_move_constructible_v<stored_type>) {
                m_storage.template emplace<stored_type>(std::move(value));
            } else {
                m_storage.template emplace<stored_type>(value);
            }
        }

        auto result() & -> decltype(auto) {
            if (std::holds_alternative<stored_type>(m_storage)) {
                if constexpr (std::is_reference_v<T>) {
                    return static_cast<T>(*std::get<stored_type>(m_storage));
                } else {
                    return static_cast<const T &>(std::get<stored_type>(m_storage));
                }

            } else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
                std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
            } else {
                throw std::runtime_error{"the coroutine doesn't have any result yet"};
            }
        }

        auto result() const & -> decltype(auto) {
            if (std::holds_alternative<stored_type>(m_storage)) {
                if constexpr (std::is_reference_v<T>) {
                    return static_cast<std::add_const<T>>(*std::get<stored_type>(m_storage));
                } else {
                    return static_cast<const T &>(std::get<stored_type>(m_storage));
                }

            } else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
                std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
            } else {
                throw std::runtime_error{"the coroutine doesn't have any result yet"};
            }
        }

        auto result() && -> decltype(auto) {
            if (std::holds_alternative<stored_type>(m_storage)) {
                if constexpr (std::is_reference_v<T>) {
                    return static_cast<T>(*std::get<stored_type>(m_storage));
                } else if constexpr (std::is_move_constructible_v<T>) {
                    return static_cast<T &&>(std::get<stored_type>(m_storage));
                } else {
                    return static_cast<const T &&>(std::get<stored_type>(m_storage));
                }

            } else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
                std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
            } else {
                throw std::runtime_error{"the coroutine doesn't have any result yet"};
            }
        }

    private:
        variant_type m_storage;
    };

    template<>  // 特化的Promise同样继承了final属性，可不显示声明
    class SyncWaitPromise<void> : public SyncWaitPromiseBase {
        using coroutine_handle = std::coroutine_handle<SyncWaitPromise<void>>;

    public:
        SyncWaitPromise() = default;

        SyncWaitPromise &operator=(SyncWaitPromise &&) = delete; // 同时会删除拷贝构造、拷贝赋值、移动构造
        ~SyncWaitPromise() = default;

    public:
        void start(SyncWaitEvent &e) {
            m_event = &e;
            coroutine_handle::from_promise(*this).resume();
        }

        auto get_return_object() noexcept { return coroutine_handle::from_promise(*this); }

        auto final_suspend() noexcept {
            struct CompleteNotifier {
                auto await_ready() noexcept { return false; }

                void await_suspend(coroutine_handle h) noexcept {
                    h.promise().m_event->set();
                }

                auto await_resume() noexcept {}
            };
            return CompleteNotifier{};
        }

        void unhandled_exception() noexcept { m_exception_ptr = std::current_exception(); }

        void return_void() {}

        void result() {
            if (m_exception_ptr) {
                std::rethrow_exception(m_exception_ptr);
            }
        }

    private:
        std::exception_ptr m_exception_ptr;
    };


    template<typename T = void>
    class SyncWaitTask {
    public:
        using promise_type = SyncWaitPromise<T>;
        using coroutine_handle = std::coroutine_handle<promise_type>;

        SyncWaitTask(coroutine_handle handle) : m_coroutine(handle) {}

        // 定义了移动操作，就不会生成拷贝构造和拷贝赋值
        SyncWaitTask(SyncWaitTask &&other) noexcept: m_coroutine(std::exchange(other.m_coroutine, nullptr)) {}

        SyncWaitTask &operator=(SyncWaitTask &&other) noexcept {
            if (std::addressof(other) != this) {
                if (m_coroutine) {
                    m_coroutine.destroy();
                }
                m_coroutine = std::exchange(other.m_coroutine, nullptr);
            }
            return *this;
        }

        ~SyncWaitTask() {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
        }

    public:
        promise_type &promise() &{ return m_coroutine.promise(); }

        const promise_type &promise() const &{ return m_coroutine.promise(); }

        promise_type &&promise() &&{ return std::move(m_coroutine.promise()); }

        auto handle() { return m_coroutine; }

    private:
        coroutine_handle m_coroutine;
    };


    template<concepts::Awaitable A, typename T = concepts::AwaitableTraits<A>::ReturnType>
    auto make_sync_wait_task(A && awaitable) -> SyncWaitTask<T> {
        if constexpr (std::is_void_v <T>) {
            co_await
            std::forward<A>(awaitable);
            co_return;
        } else {
            co_return co_await std::forward<A>(awaitable);
        }
    }
} // namespace coro::detail

namespace coro {
    template<concepts::Awaitable A, typename T = concepts::AwaitableTraits<A>::ReturnType>
    auto sync_wait(A && awaitable) -> decltype(auto) {
        detail::SyncWaitEvent e{};
        auto task = detail::make_sync_wait_task<A>(std::forward<A>(awaitable));
        task.promise().start(e);
        e.wait();
        if constexpr (std::is_void_v<T>) {
            task.promise().result();
            return;
        } else if constexpr (std::is_move_assignable_v<T>) {
            auto result = std::move(task).promise().result();
            return
            result;
        } else {
            return task.promise().result();
        }
    }
} // namespace coro

#endif //CORO_SYNC_WAIT_HPP
