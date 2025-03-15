#include "coro/net/tcp/server.hpp"


namespace coro::net::tcp {

    Server::Server(std::shared_ptr<IoScheduler> scheduler, LocalEndPoint local_end_point, uint32_t backlog)
        : m_scheduler(std::move(scheduler)), m_local_end_point(local_end_point),
          m_accept_socket(make_accept_socket(m_local_end_point.address, m_local_end_point.port, backlog)) {
        if (m_scheduler == nullptr) {
            throw std::runtime_error{"Server's IoScheduler cannot be nullptr"};
        }
    }

    Server::Server(Server&& other) noexcept
        : m_scheduler(std::move(other.m_scheduler)),
          m_local_end_point(std::move(other.m_local_end_point)), m_accept_socket(std::move(other.m_accept_socket)) {}

    Server& Server::operator=(Server&& other) noexcept {
        if (std::addressof(other) != this) {
            m_scheduler = std::move(other.m_scheduler);
            m_local_end_point = std::move(other.m_local_end_point);
            m_accept_socket = std::move(other.m_accept_socket);
        }
        return *this;
    }

    Task<PollStatus> Server::poll(std::chrono::milliseconds timeout) {
        return m_scheduler->poll(m_accept_socket, PollOp::Read, timeout);
    }

    Client Server::accept() {
        // 记录客户端的信息
        sockaddr_in clientaddr{};
        int len = sizeof(clientaddr);
        // 和客户端通信的Socket
        Socket s {::accept(m_accept_socket.fd(), (struct sockaddr*)&clientaddr, (socklen_t*)&len)};

        std::span<uint8_t> ip_addr_view = { reinterpret_cast<uint8_t*>(&clientaddr.sin_addr.s_addr), sizeof(clientaddr.sin_addr.s_addr) };

        return Client{m_scheduler, std::move(s), IpAddress{ip_addr_view}, clientaddr.sin_port };
    }

}