#include <iostream>
#include <coro/coro.hpp>

#include <memory>

using namespace coro;
using namespace coro::net;


int main() {
    auto tcp_echo_server = [] (std::shared_ptr<IoScheduler> scheduler) -> Task<> {
        auto connection_task = [] ( tcp::Client client) -> Task<> {
            std::string buf(1024, '\0');

            while (true) {
                co_await client.poll(PollOp::Read);
                auto [rstatus, rstring] = client.recv(buf);
                switch (rstatus) {
                    case RecvStatus::Ok:
                        co_await client.poll(PollOp::Write);
                        client.send(rstring);
                        break;
                    case RecvStatus::WouldBlock:
                        break;
                    case RecvStatus::Closed:
                    default:
                        co_return;
                }
            }
        };

        co_await scheduler->schedule();

        tcp::Server server {scheduler,  tcp::Server::LocalEndPoint{.port=8080}};
        while (true) {
            auto pstatus =  co_await server.poll();
            switch (pstatus) {
                case PollStatus::Event:
                {
                    auto client = server.accept();
                    if (client.socket().is_valid()) {
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

    std::vector<Task<>> workers{};
    for (int i = 0; i < 16; ++i) {
        auto scheduler= IoScheduler::make_shared(IoScheduler::Options{.execution_strategy = io_exec_thread_pool});
        workers.push_back(tcp_echo_server(scheduler));
    }

   sync_wait(when_all(std::move(workers)));
}