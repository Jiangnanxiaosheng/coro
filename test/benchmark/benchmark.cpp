#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <coro/coro.hpp>  // Main coroutine header
#include <coro/poll.hpp>  // For PollOp
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace coro;
using namespace std::chrono_literals;

// Simple coroutine task (from your original code)
Task<> simple_task() { co_return; }

// Simple coroutine for benchmarking
Task<int> simple_coro() {
    co_await std::suspend_never{};  // Minimal suspend to simulate coroutine overhead
    co_return 42;
}

// Regular function
int simple_func() { return 42; }

// Function call benchmark
void bench_function_calls(size_t iterations, bool use_coro) {
    auto start = std::chrono::high_resolution_clock::now();
    int sum = 0;
    if (use_coro) {
        for (size_t i = 0; i < iterations; ++i) {
            auto t = simple_coro();
            try {
                sum += sync_wait(t);
            } catch (...) {
                // Count exception cases but don't crash benchmark
                sum += 0;
            }
        }
    } else {
        for (size_t i = 0; i < iterations; ++i) {
            sum += simple_func();
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << (use_coro ? "Coroutine call" : "Regular function call") << ": "
              << duration_ns / iterations << " ns/call (sum=" << sum << ")\n";
}

// TCP I/O benchmark (corrected for synchronous send/recv)
Task<> bench_io_base(std::shared_ptr<IoScheduler> scheduler) {
    net::tcp::Client client(scheduler, {net::IpAddress::from_string("127.0.0.1"), 8080});
    auto start = std::chrono::high_resolution_clock::now();

    // Connect with timeout
    auto cstatus = co_await client.connect(1s);
    if (cstatus != net::ConnectStatus::Connected) {
        std::cout << "I/O base latency: Failed to connect\n";
        co_return;
    }

    // Buffer for send/recv
    std::string send_buf = "ping";
    std::vector<char> recv_buf(1024);

    // Wait for writable socket before sending
    auto pstatus = co_await client.poll(coro::PollOp::Write, 1s);  // Use coro::PollOp
    if (pstatus != PollStatus::Event) {
        std::cout << "I/O base latency: Write poll failed (" << static_cast<int>(pstatus) << ")\n";
        co_return;
    }

    // Send data (synchronous)
    auto [sstatus, remaining] = client.send(std::span<const char>{send_buf});
    if (sstatus != net::SendStatus::Ok) {
        std::cout << "I/O base latency: Send failed (" << static_cast<int>(sstatus) << ")\n";
        co_return;
    }

    // Wait for readable socket before receiving
    pstatus = co_await client.poll(coro::PollOp::Read, 1s);
    if (pstatus != PollStatus::Event) {
        std::cout << "I/O base latency: Read poll failed (" << static_cast<int>(pstatus) << ")\n";
        co_return;
    }

    // Receive data (synchronous)
    auto [rstatus, data] = client.recv(recv_buf);
    if (rstatus != net::RecvStatus::Ok) {
        std::cout << "I/O base latency: Recv failed (" << static_cast<int>(rstatus) << ")\n";
        co_return;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "I/O base latency (TCP ping): " << duration_us << " µs\n";
}

// Coroutine memory test
int coroutine_test(size_t task_count = 1'000'000) {
    // Function call benchmarks
    bench_function_calls(1'000'000, false);  // Regular function
    bench_function_calls(1'000'000, true);   // Coroutine

    // I/O benchmark (requires running TCP echo server on localhost:8080)
    auto scheduler =
        IoScheduler::make_shared(IoScheduler::Options{.execution_strategy = io_exec_thread_pool});
    sync_wait(bench_io_base(scheduler));

    return 0;
}

int main(int argc, char* argv[]) {
    coroutine_test();

    return 0;
}