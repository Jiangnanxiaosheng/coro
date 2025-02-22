//
// Created by JYH on 2025-02-22.
//

#ifndef CORO_AWAITABLE_HPP
#define CORO_AWAITABLE_HPP


#include <type_traits>
#include <concepts>
#include <iostream>
#include <coroutine>

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


template <typename T> requires Awaitable<T>
static auto get_awaiter(T&& t) {
    if constexpr (Awaiter<T>) {
        return std::forward<T>(t);
    } else if constexpr (MemberCoAwaitAwaitable<T>){
        return std::forward<T>(t).operator co_await();
    } else if constexpr (GlobalCoAwaitAwaitable<T>) {
        return operator co_await(std::forward<T>(t));
    }
}


template <Awaitable T>
struct AwaitableTraits {
    using AwaiterType = decltype(get_awaiter(std::declval<T>()));
    using ReturnType  = decltype(std::declval<AwaiterType>().await_resume());
};


#endif //CORO_AWAITABLE_HPP
