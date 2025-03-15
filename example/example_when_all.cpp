#include <coro/coro.hpp>
#include <iostream>

#include "utils.hpp"

using namespace coro;

int main() {
    printThreadId("main");

    auto task1 = []() -> Task<int> {
        printThreadId("task1");
        co_return 1;
    };

    auto task2 = []() -> Task<std::string> {
        printThreadId("task2");
        co_return "this is string";
    };

    auto task3 = []() -> Task<> {
        printThreadId("task3");
        co_return;
    };

    auto main_task = [=]() -> Task<int> {
        printThreadId("task4");
        auto [x, y, z] = co_await when_all(task1(), task2(), task3());
        static_assert(std::is_same_v<decltype(z), std::monostate>);

        printThreadId("main_task");
        std::cout << "task1 result: " << x << " " << "task2 result: " << y << "\n";
        co_return 3 + x;
    };

    auto result = sync_wait(main_task());
    std::cout << "main_task result: " << result << "\n";
    return 0;
}
