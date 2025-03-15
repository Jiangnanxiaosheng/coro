#include <fmt/core.h>

#include <coro/coro.hpp>
#include <iostream>
#include <memory>

using namespace coro;

Task<> voidTask() {
    fmt::println("这是一个没有返回值的Task");
    co_return;
}
Task<int> world() {
    fmt::println("world");
    co_await voidTask();
    fmt::println("等到voidTask返回");
    co_return 41;
}
Task<double> hello() {
    int i = co_await world();
    fmt::println("hello得到world结果为 {}", i);
    co_return i + 1.99;
}

class Fraction {
public:
    Fraction(int num, int den) : m_numerator(num), m_denominator(den) {}

    operator double() const { return static_cast<double>(m_numerator) / m_denominator; }

private:
    int m_numerator;    // 分子
    int m_denominator;  // 分母
};

int main() {
    fmt::println("开始构建hello");
    auto t = hello();
    fmt::println("构建hello完成");

    while (!t.done()) {
        t.resume();
        auto& result = t.promise().result();
        fmt::println("main得到hello的结果为: {}", result);
    }

    fmt::println("------------------------------------");
    int value{5};
    auto task_inline = [](int& x) -> Task<int&> {
        x *= 2;
        co_return x;
    }(value);

    fmt::println("value before run task_inline: {}", value);

    if (!task_inline.done()) {
        task_inline.resume();
    }

    fmt::println("value after run task_inline: {}", value);

    auto& result = task_inline.promise().result();
    std::cout << std::addressof(value) << '\n';
    std::cout << std::addressof(result) << '\n';

    fmt::println("------------------------------------");
    int val = 5;

    auto task = [](int* x) -> Task<int*> {
        *x += 100;
        co_return x;
    }(&val);

    fmt::println("rval: {}", val);
    task.resume();

    auto rval = task.promise().result();

    fmt::println("val: {}", val);
    fmt::println("rval: {}", *rval);

    val -= 100;
    fmt::println("val: {}", val);
    fmt::println("rval: {}", *rval);

    fmt::println("------------------------------------");
    int i = 42;

    auto task2 = [&i]() -> Task<int&&> { co_return std::move(i); }();

    task2.resume();

    auto iret = task2.promise().result();

    fmt::println("iret: {}", iret);

    fmt::println("------------------------------------");
    auto f = []() -> Task<double> {
        Fraction fraction(10, 3);

        co_return fraction;
    }();

    auto r = sync_wait(f);
    std::cout << r << '\n';
    std::cout << f.promise().result() << '\n';

    return 0;
}