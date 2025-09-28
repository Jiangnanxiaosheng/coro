//
// Created by yh on 3/2/25.
//

#ifndef CORO_IO_SCHEDULER_HPP
#define CORO_IO_SCHEDULER_HPP


#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include "coro/task.hpp"
#include "coro/net/poll.hpp"
#include "coro/threadpool.hpp"
#include "coro/net/socket.hpp"
#include "coro/net/poll_info.hpp"

#include "coro/sync_wait.hpp"
#include <iostream>
#include <vector>
#include <coroutine>
#include <unordered_map>
#include <unistd.h>
#include <memory>
#include <thread>

class io_scheduler {
public:
    io_scheduler() {
        epoll_fd = epoll_create1(0);
        stop_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

        epoll_event event{
                .events = EPOLLIN,
                .data = { .fd = stop_fd }
        };
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stop_fd, &event);

        worker = std::thread([this] { run(); });
    }

    ~io_scheduler() {
        stop();
        if (worker.joinable()) {
            worker.join();
        }
        if (epoll_fd != -1) {
            close(epoll_fd);
            epoll_fd = -1;
        }
        if (stop_fd != -1) {
            close(stop_fd);
            stop_fd = -1;
        }
    }

    // 运行 epoll 事件循环
    void run() {
        constexpr int MAX_EVENTS = 10;
        epoll_event events[MAX_EVENTS];

        while (running) {
            int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                if (fd == stop_fd) {
                    std::cout << "stop...\n";
                    running = false;
                    uint64_t  tmp;
                    eventfd_read(stop_fd, &tmp);
                } else {
                    std::cout << "定时器触发 fd: " << fd << "\n";
                    std::lock_guard lock(mutex);
                    auto it = waiters.find(fd);
                    if (it != waiters.end()) {
                        for (auto h: it->second)
                            h.resume();
                        waiters.erase(it);
                    }
                }
            }
        }
    }

    // 停止 epoll 事件循环
    void stop() {
        uint64_t value{1};
        eventfd_write(stop_fd, value);
    }

    struct awaitable {
        io_scheduler& scheduler;
        int fd;
        uint32_t events;
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            epoll_event event{};
            event.events = events | EPOLLONESHOT;
            event.data.fd = fd;
            epoll_ctl(scheduler.epoll_fd, EPOLL_CTL_ADD, fd, &event);

            std::lock_guard lock(scheduler.mutex);
            scheduler.waiters[fd].push_back(h);
        }

        void await_resume() {}
    };

    struct schedule_awaitable {
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            h.resume();
        }
        void await_resume() {}
    };

    awaitable poll(int fd, uint32_t events) { return {*this, fd, events}; }
    schedule_awaitable schedule() { return {}; }

private:
    int epoll_fd;
    int stop_fd;
    std::thread worker;
    std::atomic<bool> running{true};
    std::unordered_map<int, std::vector<std::coroutine_handle<>>> waiters;
    std::mutex mutex;
};

#endif //CORO_IO_SCHEDULER_HPP
