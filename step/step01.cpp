//
// Created by JYH on 2025-02-21.
//
#include <coroutine>
#include <exception>
#include <variant>
#include <format>
#include <chrono>
#include <iostream>
#include <utility>

template <typename T = void>
class Task;

class PromiseBase {
public:
    PromiseBase() = default;
    ~PromiseBase() = default;

public:
    struct FinalAwaiter {
        auto await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<>)  noexcept {
            if (handle) {
                return handle;
            }
            else {
                return std::noop_coroutine();
            }
        }

        auto await_resume() noexcept {}

        std::coroutine_handle<> handle;
    };

    auto initial_suspend() noexcept { return std::suspend_always{}; }

    auto final_suspend() noexcept { return FinalAwaiter{ m_last_handle }; }

    void continuation(std::coroutine_handle<> h) { m_last_handle = h; }

private:
    std::coroutine_handle<> m_last_handle{};
};


template <typename T>
class Promise : public PromiseBase {
public:
    Promise() = default;
    Promise(Promise&&) = delete;
    ~Promise() = default;

public:
    using variant_type = std::variant<T, std::exception_ptr>;

    auto get_return_object() noexcept;
    //auto get_return_object() noexcept { return Task<T> { std::coroutine_handle<Promise>::from_promise(*this) }; }

    auto unhandled_exception() { new(&m_storage) variant_type(std::current_exception()); }

    auto return_value(T value) { m_storage.template emplace<T>(value); }

    T result() { return std::get<T>(m_storage); }

private:
    variant_type m_storage{};
};


template <>
class Promise<void> : public PromiseBase {
public:
    Promise() = default;
    Promise(Promise&&) = delete;
    ~Promise() = default;

public:
    auto get_return_object() noexcept;
    //auto get_return_object() noexcept { return Task<void> { std::coroutine_handle<Promise>::from_promise(*this) }; }

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
class [[nodiscard]] Task {
public:
    using promise_type = Promise<T>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

public:
    Task() noexcept : m_coroutine(nullptr) {}
    explicit Task(coroutine_handle handle) noexcept : m_coroutine(handle) {}

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept {
        m_coroutine = std::exchange(other.m_coroutine, nullptr);
    }
    Task& operator=(Task&& other) noexcept {
        if (std::addressof(other) != this) {
            if (m_coroutine != nullptr) {
                m_coroutine.destroy();
            }
            m_coroutine = std::exchange(other.m_coroutine, nullptr);
        }
        return *this;
    }

    ~Task() {
        if (m_coroutine != nullptr) {
            m_coroutine.destroy();
        }
    }

public:
    coroutine_handle handle() { return m_coroutine; }
    promise_type& promise() & { return m_coroutine.promise(); }
    promise_type&& promise() && { return std::move(m_coroutine.promise()); }

    auto done() { return m_coroutine.done() || m_coroutine == nullptr; }

    auto resume() {
        if (!m_coroutine.done()) {
            m_coroutine.resume();
        }
        return !m_coroutine.done();
    }

    struct CurrentAwaiter {
        auto await_ready() { return false; }

        auto await_suspend(std::coroutine_handle<> last_handle) {
            m_current_handle.promise().continuation(last_handle);
            return m_current_handle;
        }

        auto await_resume() { return m_current_handle.promise().result(); }

        coroutine_handle m_current_handle;
    };

    auto operator co_await() { return CurrentAwaiter{ m_coroutine }; }

private:
    coroutine_handle m_coroutine;
};


template <typename T>
inline auto Promise<T>::get_return_object() noexcept {
return Task<T> { std::coroutine_handle<Promise>::from_promise(*this) };
}


inline auto Promise<void>::get_return_object() noexcept {
return Task<void> { std::coroutine_handle<Promise>::from_promise(*this) };
}


Task<> voidTask() {
    std::cout << "这是一个没有返回值的Task\n";
    co_return;
}

Task<int> world() {
    std::cout << "world\n";
    co_await voidTask();
    std::cout << "等到voidTask返回\n";
    co_return 41;
}


Task<double> hello() {
    int i = co_await world();

    std::cout << std::format("hello得到world结果为 {}\n", i);
    co_return i + 1.99;
}



int main() {
    std::cout << "开始构建hello\n";
    auto t = hello();
    std::cout << "构建hello完成\n";

    while (!t.done()) {
        t.resume();
        std::cout << "main得到hello中的结果为: " << t.promise().result() << '\n';
    }
}
