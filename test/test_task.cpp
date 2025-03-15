#include <gtest/gtest.h>

#include <coro/task.hpp>
#include <thread>

using namespace coro;

TEST(TaskTest, HandleNormalCase) {
    auto tint = []() -> Task<int> { co_return 500; }();
    auto tstr = []() -> Task<std::string> { co_return "Hello World"; }();
    auto tvoid = []() -> Task<> { co_return; }();

    EXPECT_FALSE(tint.done());
    EXPECT_FALSE(tstr.done());
    EXPECT_FALSE(tvoid.done());

    tint.resume();
    tstr.resume();
    tvoid.resume();

    EXPECT_TRUE(tint.done());
    EXPECT_TRUE(tstr.done());
    EXPECT_TRUE(tvoid.done());

    auto str = std::move(tstr).promise().result();
    EXPECT_EQ(tint.promise().result(), 500);
    EXPECT_EQ(str, "Hello World");
    EXPECT_TRUE(tstr.promise().result().empty());
}


TEST(TaskTest, HandleThrowException) {
    std::string msg = "exception occurred";
    auto task = [](const std::string& msg) -> Task<std::string> {
        throw std::runtime_error(msg);
        co_return "on exception";
    }(msg);

    task.resume();

    bool thrown{false};
    try {
        auto value = task.promise().result();
    } catch (std::runtime_error& e) {
        thrown = true;
        EXPECT_EQ(e.what(), msg);
    }

    EXPECT_TRUE(thrown);
}

TEST(TaskTest, HandleInnerTask) {
    auto outer_task = []() -> Task<> {
        std::cerr << "outer task start\n";

        auto inner_task = []() -> Task<int> {
            std::cerr << "inner task start\n";
            std::cerr << "inner task stop\n";
            co_return 5;
        };

        auto val = co_await inner_task();
        EXPECT_EQ(val, 5);
        std::cerr << "outer task stop\n";
    }();

    outer_task.resume();

    EXPECT_TRUE(outer_task.done());
}

TEST(TaskTest, HandlePassByReference) {
    int val = 5;

    auto task = [](int& x) -> Task<int&> {
        x *= 5;
        co_return x;
    }(val);

    std::cerr << "before run task\n";
    EXPECT_EQ(val, 5);

    task.resume();

    auto val_copy = task.promise().result();
    auto& rval = task.promise().result();
    std::cerr << "after run task\n";
    EXPECT_EQ(val, 25);
    EXPECT_EQ(rval, 25);
    EXPECT_EQ(val_copy, 25);

    rval += 100;
    val_copy += 1000;

    std::cerr << "only modify rval\n";
    EXPECT_EQ(val, 125);
    EXPECT_EQ(rval, 125);
    EXPECT_EQ(val_copy, 1025);

}

TEST(TaskTest, HandlePassByPoint) {
    int val = 5;

    auto task = [](int* x) -> Task<int*> {
        *x *= 5;
        co_return x;
    }(&val);

    std::cerr << "before run task\n";
    EXPECT_EQ(val, 5);

    task.resume();

    auto& pval = task.promise().result();
    std::cerr << "after run task\n";
    EXPECT_EQ(val, 25);
    EXPECT_EQ(*pval, 25);

    *pval += 100;

    std::cerr << "only modify by pval\n";
    EXPECT_EQ(val, 125);
    EXPECT_EQ(*pval, 125);
}