//
// Created by yh on 3/3/25.
//

#ifndef CORO_SERVER_HPP
#define CORO_SERVER_HPP

#include <memory>
#include <chrono>


#include "ip_address.hpp"
#include "connect_status.hpp"
#include "recv_status.hpp"
#include "send_status.hpp"
#include "socket.hpp"
#include "poll.hpp"
#include "io_scheduler.hpp"
#include "client.hpp"

#include "../task.hpp"
#include "../buffer.hpp"

class Server {
public:
    struct Options {
        //IpAddress address {IpAddress::from_string("0.0.0.0")};
        IpAddress address {0,0,0,0};
        uint16_t port {8088};
        int backlog {128};
    };

    Server(std::shared_ptr<IoScheduler> scheduler, Options opts = {.address =IpAddress::from_string("0.0.0.0"), .port = 8088, .backlog = 128}) : m_IoScheduler(std::move(scheduler)), m_options(std::move(opts)),
                                                                                                                                                 m_accept_socket(make_accept_socket(m_options.address, m_options.port, m_options.backlog)) {
        if (m_IoScheduler == nullptr) {
            throw std::runtime_error{"Server's IoScheduler cannot be nullptr"};
        }
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Server(Server&& other) noexcept : m_IoScheduler(std::move(other.m_IoScheduler)),
                                      m_options(std::move(other.m_options)), m_accept_socket(std::move(other.m_accept_socket)) {}

    Server& operator=(Server&& other) noexcept {
        if (std::addressof(other) != this) {
            m_IoScheduler = std::move(other.m_IoScheduler);
            m_options = std::move(other.m_options);
            m_accept_socket = std::move(other.m_accept_socket);
        }
        return *this;
    }

public:
    Task<PollStatus> poll(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        return m_IoScheduler->poll(m_accept_socket, PollOp::Read, timeout);
    }

    Client accept() {
        // 记录客户端的信息
        sockaddr_in clientaddr{};
        int len = sizeof(clientaddr);
        // 和客户端通信的Socket
        Socket s {::accept(m_accept_socket.fd(), (struct sockaddr*)&clientaddr, (socklen_t*)&len)};

        std::span<uint8_t> ip_addr_view = { reinterpret_cast<uint8_t*>(&clientaddr.sin_addr.s_addr), sizeof(clientaddr.sin_addr.s_addr) };
        return Client{m_IoScheduler, std::move(s), Client::Options{.address = IpAddress{ip_addr_view}, .port = ntohs(clientaddr.sin_port) }};
    }

private:
    std::shared_ptr<IoScheduler> m_IoScheduler;
    Options m_options;
    Socket m_accept_socket {-1};
};


#endif //CORO_SERVER_HPP
