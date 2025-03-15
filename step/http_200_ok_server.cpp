//
// Created by yh on 3/3/25.
//

#include <iostream>
#include "coro/net/io_scheduler.hpp"
#include "coro/net/poll_info.hpp"
#include "coro/net/poll.hpp"

#include "coro/net/connect_status.hpp"
#include "coro/net/send_status.hpp"
#include "coro/net/recv_status.hpp"

#include "coro/task.hpp"
#include "coro/sync_wait.hpp"
#include "coro/threadpool.hpp"
#include "coro/awaitable.hpp"
#include "coro/sync_wait.hpp"
#include "coro/when_all.hpp"

#include "coro/net/client.hpp"
#include "coro/net/server.hpp"

int main() {
    auto http_200_ok_server = [] (std::shared_ptr<IoScheduler> scheduler) -> Task<> {
        auto connection_task = [] (Client client) -> Task<> {
            std::string response =
                    R"(HTTP/1.1 200 OK
Content-Length: 0
Connection: keep-alive

)";
            std::string buf(1024, '\0');
            //std::cout << "处理中...\n";
            while (true) {
                co_await client.poll(PollOp::Read);
                //std::cout<< "等到可读事件\n";
                auto [rstatus, rspan] = client.recv(buf);
                //std::cout << "读到以下内容: \n";
                //std::cout.write(rspan.data(), rspan.size());


                //std::cout << "读完了\n";
                switch (rstatus) {
                    case RecvStatus::Ok:
                        // Make sure the client socket can be written to.
                        co_await client.poll(PollOp::Write);
                        client.send(std::span<const char>{response});
                        //std::cout << "send: " << response << "\n";
                        break;
                    case RecvStatus::WouldBlock:
                        break;
                    case RecvStatus::Closed:
                        std::cout << "客户端关闭\n";
                    default:
                        co_return;
                }
            }
        };

        co_await scheduler->schedule();
        std::cout << "完成了co_await scheduler->schedule();任务\n";
        Server server {scheduler};
        while (true) {
            //std::cout << "co_await server.poll()\n";
            auto pstatus =  co_await server.poll();
            //std::cout << "完成了 co_await server.poll()\n";
            switch (pstatus) {
                case PollStatus::Event:
                {
                    auto client = server.accept();
                    if (client.socket().is_valid()) {
                        std::cout << "连接成功\n";
                        scheduler->spawn(connection_task(std::move(client)));
                    }
                }
                    break;
                case PollStatus::Error:
                case PollStatus::Closed:
                case PollStatus::Timeout:
                default:
                    co_return;
            }
        }
        co_return;
    };


//    std::vector<Task<>> workers{};
//    for (int i = 0; i < 16; ++i) {
//        auto scheduler= IoScheduler::make_shared(IoScheduler::Options{.execution_strategy = iosched_exec_pool});
//        workers.push_back(http_200_ok_server(scheduler));
//    }
//
//
//    sync_wait(when_all(std::move(workers)));
    auto scheduler= IoScheduler::make_shared(IoScheduler::Options{.execution_strategy = iosched_exec_pool});
    sync_wait(http_200_ok_server(scheduler));
}