#include <coro/coro.hpp>
#include <iostream>
#include <memory>

using namespace coro;

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
    std::cout << "hello得到world结果为: " << i << "\n";
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
    std::cout << "开始构建hello\n";
    auto t = hello();
    std::cout << "构建hello完成\n";

    while (!t.done()) {
        t.resume();
        auto& result = t.promise().result();
        std::cout << "main得到hello的结果为: " <<  result << "\n";
    }

    std::cout << "------------------------------------\n";
    int value{5};
    auto task_inline = [](int& x) -> Task<int&> {
        x *= 2;
        co_return x;
    }(value);

    std::cout << "value before run task_inline: " << value << "\n";

    if (!task_inline.done()) {
        task_inline.resume();
    }

    std::cout << "value after run task_inline: " << value << "\n";

    auto& result = task_inline.promise().result();
    std::cout << std::addressof(value) << '\n';
    std::cout << std::addressof(result) << '\n';

    std::cout << "------------------------------------\n";
    int val = 5;

    auto task = [](int* x) -> Task<int*> {
        *x += 100;
        co_return x;
    }(&val);

    std::cout << "rval: " << val << "\n";
    task.resume();

    auto rval = task.promise().result();

    std::cout << "val: " << val << "\n";
    std::cout << "rval: " <<  *rval << "\n";

    val -= 100;

    std::cout << "after val-=100\n";

    std::cout << "val: " << val << "\n";
    std::cout << "rval: " <<  *rval << "\n";

    std::cout << "------------------------------------\n";
    int i = 42;

    auto task2 = [&i]() -> Task<int&&> { co_return std::move(i); }();

    task2.resume();

    auto iret = task2.promise().result();

    std::cout << "iret: " <<  iret << "\n";

    std::cout << "------------------------------------\n";
    auto f = []() -> Task<double> {
        Fraction fraction(10, 3);

        co_return fraction;
    }();

    auto r = sync_wait(f);
    std::cout << r << '\n';
    std::cout << f.promise().result() << '\n';

    return 0;
}