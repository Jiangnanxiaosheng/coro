#include <gtest/gtest.h>

#include <coro/coro.hpp>
#include "../example/utils.hpp"

using namespace coro;

TEST(ThreadPoolTest, OneWorkerOneTask) {
    ThreadPool tp(1);

    auto func = [](ThreadPool& tp) -> Task<int> {
        co_await tp.schedule();
        printThreadId("func");
        co_return 42;
    };

    auto result = coro::sync_wait(func(tp));
    EXPECT_EQ(result, 42);
}

TEST(ThreadPoolTest, OneWorkerMulitTask) {
    ThreadPool tp(1);

    auto func = [](ThreadPool& tp) -> Task<int> {
        co_await tp.schedule();
        printThreadId("func");
        co_return 42;
    };

    auto result = coro::sync_wait(func(tp));
    EXPECT_EQ(result, 42);
}
