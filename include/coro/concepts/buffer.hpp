//
// Created by yh on 3/7/25.
//

#ifndef CORO_BUFFER_HPP
#define CORO_BUFFER_HPP

#include <concepts>
#include <cstddef>  // std::size_t
#include <type_traits>

namespace coro::concepts {

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
}

#endif //CORO_BUFFER_HPP
