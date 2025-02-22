//
// Created by JYH on 2025-02-22.
//

#include <type_traits>
#include <concepts>
#include <iostream>

#include "coro/task.hpp"

template <typename T, typename... Ts>
concept InTypes = (std::same_as<T, Ts> || ...);

template <typename T>
concept Awaiter = requires(T t, std::coroutine_handle<> h) {
    { t.await_ready() } -> std::same_as<bool>;
    { t.await_suspend(h) } -> InTypes<void, bool, std::coroutine_handle<>>;
    { t.await_resume() };
};

template <typename T>
concept MemberCoAwaitAwaitable = requires(T t) {
    { t.operator co_await() } -> Awaiter;
};

template <typename T>
concept GlobalCoAwaitAwaitable = requires(T t) {
    { operator co_await(t) } -> Awaiter;
};

template <typename T>
concept Awaitable = Awaiter<T> || MemberCoAwaitAwaitable<T> || GlobalCoAwaitAwaitable<T>;

template <typename T> requires Awaitable<T>    // constraint writing method 1
static auto get_awaiter(T&& t) {
    if constexpr (Awaiter<T>) {
        return std::forward<T>(t);
    } else if constexpr (MemberCoAwaitAwaitable<T>){
        return std::forward<T>(t).operator co_await();
    } else if constexpr (GlobalCoAwaitAwaitable<T>) {
        return operator co_await(std::forward<T>(t));
    }
}

template <Awaitable T>     // constraint writing method 2
struct AwaitableTraits {
    using AwaiterType = decltype(get_awaiter(std::declval<T>()));
    using ReturnType  = decltype(std::declval<AwaiterType>().await_resume());
};

namespace test {
    struct Awaiter {
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<>) {}
        int await_resume() { return 99; }
    };

    struct CoAwaitAwaitableTask {
        auto operator co_await(){
            return Awaiter{};
        }
    };

    struct NonAwaitable {

    };
}

int main() {
    std::cout << std::boolalpha;
    std::cout << Awaiter<test::Awaiter> << "\n";
    std::cout << Awaitable<test::Awaiter> << "\n";

    std::cout << Awaiter<test::CoAwaitAwaitableTask> << "\n";
    std::cout << Awaitable<test::CoAwaitAwaitableTask> << "\n";

    std::cout << Awaiter<test::NonAwaitable> << "\n";
    std::cout << Awaitable<test::NonAwaitable> << "\n";

    using awaiter_type = typename AwaitableTraits<test::CoAwaitAwaitableTask>::AwaiterType ;
    using return_type =  typename AwaitableTraits<test::CoAwaitAwaitableTask>::ReturnType;
    static_assert(std::is_same_v<awaiter_type, test::Awaiter>);
    static_assert(std::is_same_v<return_type, int>);

    return 0;
}