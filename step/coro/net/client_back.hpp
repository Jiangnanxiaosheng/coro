//
// Created by yh on 3/3/25.
//

#ifndef CORO_CLIENT_HPP
#define CORO_CLIENT_HPP

#include <memory>
#include <optional>
#include <chrono>


#include "ip_address.hpp"
#include "connect_status.hpp"
#include "recv_status.hpp"
#include "send_status.hpp"
#include "socket.hpp"
#include "poll.hpp"
#include "io_scheduler.hpp"

#include "../task.hpp"
#include "../buffer.hpp"

class Server;

class Client {
    friend class Server;
public:
    struct Options {
        IpAddress address {IpAddress::from_string("127.0.0.1")};
        uint16_t port {8088};
    };

    Client(std::shared_ptr<IoScheduler> scheduler, Options opts = Options{.address = IpAddress::from_string("127.0.0.1"), .port = 8088}) : m_IoScheduler(std::move(scheduler)),
                                                                                                                                           m_options(opts), m_socket(make_nonblock_socket()) {
        if (m_IoScheduler == nullptr) {
            throw std::runtime_error{"Client's IoScheduler cannot be nullptr"};
        }
    }

    Client(const Client& other) : m_IoScheduler(other.m_IoScheduler), m_options(other.m_options),
                                  m_socket(other.m_socket), m_connect_status(other.m_connect_status) {}

    Client& operator=(const Client& other) {
        if (std::addressof(other) != this) {
            m_IoScheduler = other.m_IoScheduler;
            m_options = other.m_options;
            m_socket = other.m_socket;
            m_connect_status = other.m_connect_status;
        }
        return *this;
    }

    Client(Client&& other) noexcept : m_IoScheduler(std::move(other.m_IoScheduler)), m_options(std::move(other.m_options)),
                                      m_socket(std::move(other.m_socket)), m_connect_status(std::exchange(other.m_connect_status, std::nullopt)) {}

    Client& operator=(Client&& other) noexcept {
        if (std::addressof(other) != this) {
            m_IoScheduler = std::move(other.m_IoScheduler);
            m_options = std::move(other.m_options);
            m_socket = std::move(other.m_socket);
            m_connect_status = std::exchange(other.m_connect_status, std::nullopt);
        }
        return *this;
    }

    ~Client() {}

public:
    Socket& socket() { return m_socket; }
    const Socket& socket() const { return m_socket; }

    Task<ConnectStatus>   connect(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        if (m_connect_status.has_value()) {
            co_return m_connect_status.value();
        }

        // 状态更新函数
        auto return_value = [this](ConnectStatus s) {
            m_connect_status = s;
            return s;
        };

        sockaddr_in clientaddr{};
        clientaddr.sin_family = AF_INET;
        clientaddr.sin_port = htons(m_options.port);
        clientaddr.sin_addr = *reinterpret_cast<const in_addr*>(m_options.address.data().data());

        auto cret = ::connect(m_socket.fd(), (struct sockaddr*)&clientaddr, sizeof(clientaddr));
        if (cret == 0) {
            co_return return_value(ConnectStatus::Connected);

            // 连接未立即完成（非阻塞模式）
        } else if (cret == -1) {
            if (errno == EAGAIN || errno == EINPROGRESS) {
                // 等待可写事件
                auto pstatus = co_await m_IoScheduler->poll(m_socket, PollOp::Write, timeout);
                if (pstatus == PollStatus::Event) {
                    int result {0};
                    socklen_t len {sizeof(result)};
                    if (getsockopt(m_socket.fd(), SOL_SOCKET, SO_ERROR, &result, &len)) {
                        std::cerr << "connect failed to getsockopt after write poll event\n";
                    }

                    if (result == 0) {
                        co_return return_value(ConnectStatus::Connected);
                    }

                } else if (pstatus == PollStatus::Timeout) {
                    co_return return_value(ConnectStatus::Timeout);
                }
            }
        }
        co_return return_value(ConnectStatus::Error);
    }

    Task<PollStatus> poll(PollOp op, std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        return m_IoScheduler->poll(m_socket, op, timeout);
    }


    // 返回接收状态和实际接收的数据段
    auto recv(MutableBuffer auto&& buffer) -> std::pair<RecvStatus, std::span<char>> {
        if (buffer.empty()) {
            return {RecvStatus::Ok, std::span<char>{}};
        }

        auto bytes_recv = ::recv(m_socket.fd(), buffer.data(), buffer.size(), 0);
        if (bytes_recv > 0) {
            return {RecvStatus::Ok, std::span<char>{buffer.data(), static_cast<size_t>(bytes_recv)}};
        } else if (bytes_recv == 0) {
            return {RecvStatus::Closed, std::span<char>{}};
        } else {
            return {static_cast<RecvStatus>(errno), std::span<char>{}};
        }
    }

    // 返回发送状态和未发送的剩余数据
    auto send(const ConstBuffer auto&  buffer) -> std::pair<SendStatus, std::span<const char>> {
        if (buffer.empty()) {
            return {SendStatus::Ok, std::span<const char>{buffer.data(), buffer.size()}};
        }

        auto bytes_sent = ::send(m_socket.fd(), buffer.data(), buffer.size(), 0);
        if (bytes_sent >= 0) {
            return {SendStatus::Ok, std::span<const char>{buffer.data() + bytes_sent, buffer.size() - bytes_sent}};
        } else {
            return {static_cast<SendStatus>(errno), std::span<const char>{buffer.data(), buffer.size()}};
        }
    }

private:
    // 由 Server调用 accept() 创建用于和客户端通信的 Client
    Client(std::shared_ptr<IoScheduler> scheduler, Socket socket, Options opts) : m_IoScheduler(std::move(scheduler)),
                                                                                  m_options(std::move(opts)), m_socket(std::move(socket)), m_connect_status(ConnectStatus::Connected) {
        m_socket.set_blocking(false);
    }

    Options m_options;
    std::shared_ptr<IoScheduler> m_IoScheduler;
    Socket m_socket {};
    std::optional<ConnectStatus> m_connect_status;
};


#endif //CORO_CLIENT_HPP
