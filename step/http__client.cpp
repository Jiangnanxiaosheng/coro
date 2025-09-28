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

#include "coro/net/client_back.hpp"
#include "coro/net/server.hpp"

int main() {
    auto http_200_ok_server = [] (std::shared_ptr<IoScheduler> scheduler) -> Task<> {

//        std::string ss {"GET / HTTP/1.1\r\n"
//                        "Connection: Keep-Alive\r\n"
//                        "Host: 127.0.0.1:8088\r\n"
//                        "User-Agent: ApacheBench/2.3\r\n"
//                        "Accept: */*\r\n\r\n"};
        std::string ss = "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 111 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 222 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 333 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 444 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 555 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 666 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 777 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 888 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 999 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 111 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 222 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 333 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 444 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 555 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 666 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 777 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 888 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 999 "
                         "dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao dajiahao 000 ";
        std::cout << "size: " << ss.size() << "\n";
        co_await scheduler->schedule();

        Client client {scheduler, {.address = IpAddress::from_string("127.0.0.1"), .port = 8080}};

        auto cstatus =  co_await client.connect();
        switch (cstatus) {
                case ConnectStatus::Connected:
                {
                    std::cout << "客户端连接服务器成功\n";
                    uint16_t length = htons(ss.size());  // 使用 htonl 而不是 htons
                    std::string tmp;
                    tmp.append(reinterpret_cast<const char*>(&length), sizeof(length));
                    tmp += ss;
                    auto [sstatus, remaining] = client.send(tmp);
                    if (!remaining.empty()) {
                        std::cout << "客户端没有发送完, 剩下: ";
                        for (auto c: remaining) {
                            std::cout << c;
                        }
                    } else {
                        std::cout << "客户端发送完毕\n";
                    }

                }
                    break;
                case ConnectStatus::Error:
                case ConnectStatus::Timeout:
                default:
                    std::cout << "连接失败\n";
                    co_return;
        }

        auto pstatus = co_await client.poll(PollOp::Read);

        std::string buf(2048, '\0');

        auto [rstatus, rstring] = client.recv(buf);
        std::cout << to_string(rstatus) << "\n";
        std::cout << "客户端接收到:\n";
        for (auto c : rstring) {
            std::cout << c;
        }
        sleep(1);
        co_return;
    };


    auto scheduler= IoScheduler::make_shared(IoScheduler::Options{.execution_strategy = iosched_exec_pool});
    sync_wait(http_200_ok_server(scheduler));
}