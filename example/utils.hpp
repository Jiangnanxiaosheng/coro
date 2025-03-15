#ifndef CORO_UTILS_HPP
#define CORO_UTILS_HPP

#include <iostream>
#include <format>
#include <thread>
#include <sstream>


inline void printThreadId(const std::string& str) {
    auto thread_id = std::this_thread::get_id();
    // 将 thread_id 转换为字符串
    std::ostringstream oss;
    oss << thread_id;
    std::string thread_id_str = oss.str();

    std::string res = "ThreadID[" + thread_id_str + "] : " + str;

    std::cout << res << "\n";
}

#endif //CORO_UTILS_HPP
