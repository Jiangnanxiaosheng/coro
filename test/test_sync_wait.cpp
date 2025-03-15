#include <gtest/gtest.h>

#include <coro/coro.hpp>

using namespace coro;

TEST(SyncWaitTest, handleNormalCase) {
    auto func = []() -> Task<int> { co_return 11; };

    auto result = coro::sync_wait(func());
    EXPECT_EQ(result, 11);
}

TEST(SyncWaitTest, handleReturnVoid) {
    std::string output;

    auto func = [&]() -> Task<void> {
        output = "hello from sync_wait<void>\n";
        co_return;
    };

    coro::sync_wait(func());
    EXPECT_EQ(output, "hello from sync_wait<void>\n");
}