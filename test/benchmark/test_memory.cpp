#include <chrono>
#include <coro/coro.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace coro;

std::size_t getMemoryUsage() {
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("VmRSS:") != std::string::npos) {
            std::istringstream iss(line);
            std::string key;
            std::size_t value;
            std::string unit;
            iss >> key >> value >> unit;

            return value;  // 单位为 KB
        }
    }

    return 0;
}

Task<> simple_task() { co_return; }

int coroutine_test() {
    const size_t task_count = 1'000'000;
    std::cout << "coroutine memory test for: " << task_count << " coroutines\n";
    size_t mem_before = getMemoryUsage();
    std::cout << "任务创建前内存： " << mem_before << " kB\n";

    std::vector<Task<void>> tasks;
    tasks.reserve(task_count);
    for (size_t i = 0; i < task_count; ++i) {
        tasks.emplace_back(simple_task());
    }

    size_t mem_after_creation = getMemoryUsage();
    std::cout << "任务创建后内存： " << mem_after_creation << " kB\n";

    // 使用 when_all 将所有任务合并，然后同步等待完成
    auto all_tasks = when_all(std::move(tasks));
    sync_wait(all_tasks);

    size_t mem_after_completion = getMemoryUsage();
    std::cout << "所有任务完成后内存： " << mem_after_completion << " kB\n";

    double overhead_bytes = (mem_after_creation > mem_before)
                                ? (mem_after_creation - mem_before) * 1024.0 / task_count
                                : 0;
    std::cout << "每个协程平均内存开销（任务创建阶段）： " << overhead_bytes << " 字节\n";

    return 0;
}

void thread_function() {}

// 线程测试：创建 1000 个线程
int thread_test() {
    const size_t thread_count = 1000;
    std::cout << "thread memory test for: " << thread_count << " threads\n";
    size_t mem_before = getMemoryUsage();
    std::cout << "线程创建前内存： " << mem_before << " kB\n";

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back(thread_function);
    }
    for (auto& th : threads) {
        th.join();
    }

    size_t mem_after = getMemoryUsage();
    std::cout << "所有线程结束后内存： " << mem_after << " kB\n";

    double overhead_bytes =
        (mem_after > mem_before) ? (mem_after - mem_before) * 1024.0 / thread_count : 0;
    std::cout << "每个线程平均内存开销（测试值）： " << overhead_bytes << " 字节\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " [coroutine|thread]\n";
        return 1;
    }
    std::string mode = argv[1];
    if (mode == "coroutine") {
        return coroutine_test();
    } else if (mode == "thread") {
        return thread_test();
    } else {
        std::cout << "无效的模式。请选择 'coroutine' 或 'thread'.\n";
        return 1;
    }

    return 0;
}