//
// Created by yh on 3/3/25.
//

#ifndef CORO_BUFFER_HPP
#define CORO_BUFFER_HPP

#include <concepts>
#include <type_traits>

template <typename T>
concept MutableBuffer = requires(T t) {
    { t.empty() } -> std::same_as<bool>;
    { t.data() } -> std::same_as<char*>;
    { t.size() } -> std::same_as<std::size_t>;
};

template <typename T>
concept ConstBuffer = requires(const T t) {
    { t.empty() } -> std::same_as<bool>;
    { t.data() } -> std::same_as<const char*>;
    { t.size() } -> std::same_as<std::size_t>;
};


#endif //CORO_BUFFER_HPP
