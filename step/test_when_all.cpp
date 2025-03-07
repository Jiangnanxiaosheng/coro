//
// Created by yh on 3/5/25.
//

#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <coroutine>
#include <chrono>
#include <queue>
#include <tuple>
#include <condition_variable>

#include "coro/task.hpp"
#include "coro/sync_wait.hpp"
#include "coro/when_all.hpp"

int main() {
    std::cout << "main线程id " << std::this_thread::get_id() << "\n";

    auto task1 = []() -> Task<int> {
        std::cout << "task1线程id " << std::this_thread::get_id() << "\n";
        co_return 1;
    };

    auto task12 = []() -> Task<int> {
        std::cout << "task12线程id " << std::this_thread::get_id() << "\n";
        co_return 12;
    };

    auto task2 = []() -> Task<std::string> {
        std::cout << "task2线程id " << std::this_thread::get_id() << "\n";
        co_return "this is string";
    };

    auto task3 = []() -> Task<> {
        std::cout << "task3线程id " << std::this_thread::get_id() << "\n";
        co_return;
    };

    auto task4 = [=]() -> Task<int> {
        std::cout << "task4线程id " << std::this_thread::get_id() << "\n";
        auto [x, y, z] = co_await when_all(task1(), task2(), task3());
        static_assert(std::is_same_v<decltype(z), std::monostate>);

        std::cout << "task4: co_await结束\n";
        std::cout << "task1 result: " << x << " " << "task2 result: " << y << "\n";
        co_return 3;
    };

    std::vector<Task<int>> vec{};
    vec.push_back(task1());
    vec.push_back(task12());

    auto result = sync_wait(when_all(std::move(vec)));
    //auto result = sync_wait(task4());
    for (auto x : result) {
        std::cout << x << " ";
    }
    return 0;
}