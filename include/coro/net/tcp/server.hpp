#ifndef CORO_SERVER_HPP
#define CORO_SERVER_HPP

#include "coro/net/tcp/client.hpp"

namespace coro::net::tcp {

    class Client;

    class Server {
    public:
        struct LocalEndPoint {
            IpAddress address;
            uint16_t port;
        };

        Server(std::shared_ptr<IoScheduler> scheduler,LocalEndPoint local_end_point = {.address = IpAddress::from_string("0.0.0.0"), .port = 8080 }, uint32_t backlog = 128);

        Server(Server&& other) noexcept;
        Server& operator=(Server&& other) noexcept;
        ~Server() = default;

    public:
        Task<PollStatus> poll(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

        Client accept();

    private:
        std::shared_ptr<IoScheduler> m_scheduler;
        LocalEndPoint m_local_end_point;
        Socket m_accept_socket {-1};

    };


} // namespce coro::net::tcp


#endif //CORO_SERVER_HPP
