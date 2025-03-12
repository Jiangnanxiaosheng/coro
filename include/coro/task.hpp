//
// Created by yh on 3/7/25.
//

#ifndef CORO_TASK_HPP
#define CORO_TASK_HPP

#include <coroutine>
#include <stdexcept>
#include <exception>
#include <variant>
#include <type_traits>
#include <utility>

namespace coro::detail {

    struct PromiseBase {
        struct ReturnPreviousAwaiter {
            auto await_ready() noexcept { return false; }
            /*
             * Todo 如果之前的协程没有结束，就恢复之前的协程
             */
            auto await_suspend(std::coroutine_handle<> h) noexcept -> std::coroutine_handle<> {
                if (previousHandle) {
                    return previousHandle;
                } else {
                    return std::noop_coroutine();
                }
            }
            auto await_resume() noexcept {}

            std::coroutine_handle<> previousHandle;
        };

        PromiseBase() = default;
        virtual ~PromiseBase() = default;

        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return ReturnPreviousAwaiter{m_previousHandle}; }

        void continuation(std::coroutine_handle<> h) { m_previousHandle = h; }

    protected:
        std::coroutine_handle<> m_previousHandle;
    };

    template<typename T>
    class Promise final : public PromiseBase {
        // 支持存储引用类型，如果T是引用，那么存储移除引用限定类型的指针，否则存储移除const限定类型
        using stored_type = std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T>*, std::remove_const_t<T>>;
        using variant_type = std::variant<std::monostate, stored_type, std::exception_ptr>;
        using coroutine_handle = std::coroutine_handle<Promise<T>>;

    public:
        Promise() = default;
        Promise& operator=(Promise &&) = delete; // 同时会删除拷贝构造、拷贝赋值、移动构造
        ~Promise() = default;

    public:
        auto get_return_object() noexcept;

        void unhandled_exception() noexcept {
            m_storage.template emplace<std::exception_ptr>(std::current_exception());
        }

        template<typename ValueType>
//        requires requires {
//            requires std::is_reference_v<T> && std::is_constructible_v<T, ValueType &&>;
//        } || requires {
//            requires (not std::is_reference_v<T>) && std::is_constructible_v<stored_type, ValueType &&>;
//        }
        requires(std::is_reference_v<T> and std::is_constructible_v<T, ValueType&&>) or
                (not std::is_reference_v<T> and std::is_constructible_v<stored_type, ValueType&&>)
        void return_value(ValueType&& value) {
            if constexpr (std::is_reference_v<T>) {
                T ref = static_cast<ValueType &&>(value);
                m_storage.template emplace<stored_type>(std::addressof(ref));

            } else {
                m_storage.template emplace<stored_type>(std::forward<ValueType>(value));
            }
        }

        void return_value(stored_type &&value) requires(not std::is_reference_v<T>) {
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
    class Promise<void> : public PromiseBase {
        using coroutine_handle = std::coroutine_handle<Promise<void>>;

    public:
        Promise() = default;
        Promise &operator=(Promise &&) = delete; // 同时会删除拷贝构造、拷贝赋值、移动构造
        ~Promise() = default;

    public:
        auto get_return_object() noexcept;

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

} // namespace coro::detail;

namespace coro {
    template<typename T = void>
    class [[nodiscard("can't ignore return_value as Task")]] Task {
    public:
        using promise_type = detail::Promise<T>;
        using coroutine_handle = std::coroutine_handle<promise_type>;

        Task() = default;
        explicit Task(coroutine_handle handle) : m_coroutine(handle) {}
        // 定义了移动操作，就不会生成拷贝构造和拷贝赋值
        Task(Task && other) noexcept : m_coroutine(std::exchange(other.m_coroutine, nullptr)) {}
        Task& operator=(Task && other) noexcept {
            if (std::addressof(other) != this) {
                if (m_coroutine) {
                    m_coroutine.destroy();
                }
                m_coroutine = std::exchange(other.m_coroutine, nullptr);
            }
            return *this;
        }
        ~Task() {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
        }

    public:
        /*
         * @Todo 检查任务是否执行完毕
         * @return true if task is over
         */
        bool done() { return m_coroutine == nullptr || m_coroutine.done(); }

        void resume() {
            if (m_coroutine) {
                m_coroutine.resume();
            }
        }

        void destroy() {
            if (m_coroutine) {
                m_coroutine.destroy();
                m_coroutine = nullptr;
            }
        }

        auto operator co_await() {
            struct Awaiter {
                bool await_ready() noexcept { return false; }
                /*
                 * Todo 保存之前所在的协程的句柄，然后执行co_await的协程
                 */
                auto await_suspend(std::coroutine_handle<> h) noexcept -> std::coroutine_handle<> {
                    m_currentHandle.promise().continuation(h);
                    return m_currentHandle;
                }
                auto await_resume() noexcept { return m_currentHandle.promise().result(); }

                coroutine_handle m_currentHandle;
            };

            return Awaiter{m_coroutine};
        }

        promise_type &promise() & { return m_coroutine.promise(); }
        const promise_type &promise() const& { return m_coroutine.promise(); }
        promise_type &&promise() && { return std::move(m_coroutine.promise()); }

        auto handle() { return m_coroutine; }

    private:
        coroutine_handle m_coroutine;
    };

    namespace detail {
        template<typename T>
        inline auto Promise<T>::get_return_object() noexcept {
            return Task<T>{coroutine_handle::from_promise(*this)};
        }

        inline auto Promise<void>::get_return_object() noexcept {
            return Task<void>{coroutine_handle::from_promise(*this)};
        }

    } // namespace coro::detail

} // namespace coro

#endif //CORO_TASK_HPP
