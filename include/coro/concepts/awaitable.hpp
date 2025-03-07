//
// Created by yh on 3/7/25.
//

#ifndef CORO_AWAITABLE_HPP
#define CORO_AWAITABLE_HPP

#include <concepts>
#include <coroutine>
#include <type_traits>
#include <utility>  // std::forward<>()

namespace coro::concepts {

    // 检查一个类型 T 是否与类型列表 Ts... 中的任何一个类型相同
    template<typename T, typename... Ts>
    concept InTypes = (std::same_as<T, Ts> || ...);

    template<typename T>
    concept Awaiter = requires(T t, std::coroutine_handle<> h) {
        { t.await_ready() } -> std::same_as<bool>;
        { t.await_suspend(h) } -> InTypes<void, bool, std::coroutine_handle<>>;
        { t.await_resume() };
    };

    template<typename T>
    concept MemberCoAwaitAwaitable = requires(T t) {
        { t.operator co_await() } -> Awaiter;
    };

    template<typename T>
    concept GlobalCoAwaitAwaitable = requires(T t) {
        { operator co_await(t) } -> Awaiter;
    };

    template<typename T>
    concept Awaitable = Awaiter<T> || MemberCoAwaitAwaitable<T> || GlobalCoAwaitAwaitable<T>;

    template<Awaitable A>
    static auto get_awaiter(A &&a) {
        if constexpr (Awaiter<A>) {
            return std::forward<A>(a);
        } else if constexpr (MemberCoAwaitAwaitable<A>) {
            return std::forward<A>(a).operator co_await();
        } else if constexpr (GlobalCoAwaitAwaitable<A>) {
            return operator co_await(std::forward<A>(a));
        }
    }

    template<Awaitable A>
    struct AwaitableTraits {
        using AwaiterType = decltype(get_awaiter(std::declval<A>()));
        using ReturnType = decltype(std::declval<AwaiterType>().await_resume());
    };

} // namespace coro::concepts

#endif //CORO_AWAITABLE_HPP
