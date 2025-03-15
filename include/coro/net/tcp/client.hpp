#ifndef CORO_CLIENT_HPP
#define CORO_CLIENT_HPP

#include "coro/task.hpp"
#include "coro/io_scheduler.hpp"
#include "coro/net/recv_status.hpp"
#include "coro/net/send_status.hpp"
#include "coro/net/connect_status.hpp"
#include "coro/net/ip_address.hpp"
#include "coro/net/socket.hpp"

#include "coro/concepts/buffer.hpp"

#include <chrono>
#include <memory>
#include <optional>

namespace coro::net::tcp {

    class Client {
        friend class Server;
    public:
        struct RemoteEndPoint {
            IpAddress address;
            uint16_t port;
        };

        Client(std::shared_ptr<IoScheduler> scheduler, RemoteEndPoint remote_end_point =
                RemoteEndPoint{.address = IpAddress::from_string("127.0.0.1"), .port = 8080});

        Client(const Client& other);
        Client& operator=(const Client& other);
        Client(Client&& other) noexcept;
        Client& operator=(Client&& other) noexcept;
        ~Client();

    public:
        Task<PollStatus> poll(PollOp op, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

        Task<ConnectStatus> connect(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

        // 返回接收状态和实际接收的数据段
        auto recv(concepts::MutableBuffer auto&& buffer) -> std::pair<RecvStatus, std::string> {
            if (buffer.empty()) {
                return {RecvStatus::Ok, std::string{}};
            }

            auto bytes_recv = ::recv(m_socket.fd(), buffer.data(), buffer.size(), 0);
            if (bytes_recv > 0) {
                return {RecvStatus::Ok, std::string{buffer.data(), static_cast<size_t>(bytes_recv)}};
            } else if (bytes_recv == 0) {
                return {RecvStatus::Closed, std::string{}};
            } else {
                return {static_cast<RecvStatus>(errno), std::string{}};
            }
        }

        // 返回发送状态和未发送的剩余数据
        auto send(const concepts::ConstBuffer auto&  buffer) -> std::pair<SendStatus, std::string> {
            if (buffer.empty()) {
                return {SendStatus::Ok, std::string{buffer.data(), buffer.size()}};
            }

            auto bytes_sent = ::send(m_socket.fd(), buffer.data(), buffer.size(), 0);
            if (bytes_sent >= 0) {
                return {SendStatus::Ok, std::string{buffer.data() + bytes_sent, buffer.size() - bytes_sent}};
            } else {
                return {static_cast<SendStatus>(errno), std::string{buffer.data(), buffer.size()}};
            }
        }

        Socket& socket() { return m_socket; }
        const Socket socket() const { return m_socket; }
        const RemoteEndPoint& remote_endpoint() const { return m_remote_endpoint; }

    private:
        // 由 Server调用 accept() 创建用于和客户端通信的 Client
        Client(std::shared_ptr<IoScheduler> scheduler, Socket socket, IpAddress remote_ip, uint16_t remote_port);

        std::shared_ptr<IoScheduler> m_scheduler {nullptr};
        RemoteEndPoint m_remote_endpoint;
        Socket m_socket {-1};
        std::optional<ConnectStatus> m_connect_status {std::nullopt};
    };

} // namespace coro::net::tcp

#endif //CORO_CLIENT_HPP
