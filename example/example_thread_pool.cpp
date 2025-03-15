#include <coro/coro.hpp>
#include "utils.hpp"
#include <string>

using namespace coro;

int main() {
    printThreadId("main");

    ThreadPool tp(2);

    auto offload_task = [](ThreadPool& tp, int child_idx) -> Task<int> {
        std::string prefix = "offload_task[" + std::to_string(child_idx) + "]";

        printThreadId(prefix + " before scheduler");
        co_await tp.schedule();
        printThreadId(prefix + " after scheduler");

        std::size_t calculation{ 0 };
        for (std::size_t i = 0; i < 1'000'000; ++i) {
            calculation++;

            if (i > 0 && i % 250'000 == 0) {

                std::cout << prefix + " is yielding()\n";
                co_await tp.yield();
                std:: cout << prefix +  " rerunning\n";

            }
        }
        std::cout << prefix + " co_return\n";
        co_return calculation;
    };

    auto [a, x, y, z] = sync_wait(when_all(offload_task(tp, 1), offload_task(tp, 2), offload_task(tp, 3), offload_task(tp, 4)));


    std::cout << "calculated thread pool result = " << a  << " " <<  x << " " << y << " " << z << "\n";
}