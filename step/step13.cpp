//
// Created by yh on 2/27/25.
//

#include <concepts>
#include <type_traits>
#include <memory>
#include <optional>
#include <chrono>
#include <span>
#include <iostream>
#include <cassert>
#include <unordered_map>
#include "coro/net/ip_address.hpp"
#include "coro/net/connect_status.hpp"
#include "coro/net/recv_status.hpp"
#include "coro/net/send_status.hpp"
#include "coro/net/socket.hpp"
#include "coro/net/poll.hpp"
#include "coro/task.hpp"
#include "coro/when_all.hpp"
#include "coro/sync_wait.hpp"
#include "coro/net/io_scheduler.hpp"


template <typename T>
concept MutableBuffer = requires(T t) {
    { t.empty() } -> std::same_as<bool>;
    { t.data() } -> std::same_as<char*>;
    { t.size() } -> std::same_as<std::size_t>;
};

template <typename T>
concept ConstBuffer = requires(const T t) {
    { t.empty() } -> std::same_as<bool>;
    { t.data() } -> std::same_as<const char*>;
    { t.size() } -> std::same_as<std::size_t>;
};

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




int main() {
    const std::string client_msg{"Hello from client"};
    const std::string server_msg{"Reply from server!"};

    auto scheduler = IoScheduler::make_shared(
            IoScheduler::Options{.threads_count = 1});

    auto make_client_task = [](std::shared_ptr<IoScheduler>& scheduler,
          const std::string& client_msg, const std::string& server_msg) -> Task<void> {
        co_await scheduler->schedule();
        Client client{scheduler};

        std::cerr << "client connect\n";
        auto cstatus = co_await client.connect();
        assert(cstatus == ConnectStatus::Connected);

        std::cerr << "client send()\n";
        auto [sstatus, remaining] = client.send(client_msg);
        assert(sstatus == SendStatus::Ok);
        assert(remaining.empty());

        // Poll for the server's response.
        std::cerr << "client poll(read)\n";
        auto pstatus = co_await client.poll(PollOp::Read);
        assert(pstatus == PollStatus::Event);

        std::string buffer(256, '\0');
        std::cerr << "client recv()\n";
        auto [rstatus, rspan] = client.recv(buffer);
        assert(rstatus == RecvStatus::Ok);
        assert(rspan.size() == server_msg.length());
        buffer.resize(rspan.size());
        assert(buffer == server_msg);

        std::cerr << "client return\n";
        co_return;
    };

    auto make_server_task = [](std::shared_ptr<IoScheduler>& scheduler,
            const std::string&  client_msg, const std::string& server_msg) -> Task<> {
        co_await scheduler->schedule();
        Server server{scheduler};

        // Poll for client connection.
        std::cerr << "server poll(accept)\n";
        auto pstatus = co_await server.poll();
        assert(pstatus == PollStatus::Event);
        std::cerr << "server accept()\n";
        auto client = server.accept();
        assert(client.socket().is_valid());

        // Poll for client request.
        std::cerr << "server poll(read)\n";
        pstatus = co_await client.poll(PollOp::Read);
        assert(pstatus == PollStatus::Event);

        std::string buffer(256, '\0');
        std::cerr << "server recv()\n";
        auto [rstatus, rspan] = client.recv(buffer);
        assert(rstatus == RecvStatus::Ok);
        assert(rspan.size() == client_msg.size());
        buffer.resize(rspan.size());
        assert(buffer == client_msg);

        // Respond to client.
        std::cerr << "server send()\n";
        auto [sstatus, remaining] = client.send(server_msg);
        assert(sstatus == SendStatus::Ok);
        assert(remaining.empty());

        std::cerr << "server return\n";
        co_return;
    };
    
    sync_wait(when_all(
            make_server_task(scheduler, client_msg, server_msg), 
            make_client_task(scheduler, client_msg, server_msg)));
    
    return 0;
}