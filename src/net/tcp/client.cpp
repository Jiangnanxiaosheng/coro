#include "coro/net/tcp/client.hpp"


namespace coro::net::tcp {

    Client::Client(std::shared_ptr<IoScheduler> scheduler, RemoteEndPoint remote_end_point)
        : m_scheduler(std::move(scheduler)), m_remote_endpoint(remote_end_point), m_socket(make_nonblocking_socket()) {
        if (m_scheduler == nullptr) {
            throw std::runtime_error{"tcp::Client cannot have nullptr IoScheduler"};
        }
    }

    Client::Client(const Client& other)
        : m_scheduler(other.m_scheduler),
          m_remote_endpoint(other.m_remote_endpoint),
          m_socket(other.m_socket), m_connect_status(other.m_connect_status) {}

    Client& Client::operator=(const Client& other) {
        if (std::addressof(other) != this) {
            m_scheduler = other.m_scheduler;
            m_remote_endpoint = other.m_remote_endpoint;
            m_socket = other.m_socket;
            m_connect_status = other.m_connect_status;
        }
        return *this;
    }

    Client::Client(Client&& other) noexcept
        : m_scheduler(std::move(other.m_scheduler)),
          m_remote_endpoint(std::move(other.m_remote_endpoint)),
          m_socket(std::move(other.m_socket)),
          m_connect_status(std::exchange(other.m_connect_status, std::nullopt)) {}

    Client& Client::operator=(Client&& other) noexcept {
        if (std::addressof(other) != this) {
            m_scheduler = std::move(other.m_scheduler);
            m_remote_endpoint = std::move(other.m_remote_endpoint);
            m_socket = std::move(other.m_socket);
            m_connect_status = std::exchange(other.m_connect_status, std::nullopt);
        }
        return *this;
    }

    Client::~Client() {}

    Task<PollStatus> Client::poll(PollOp op, std::chrono::milliseconds timeout) {
        return m_scheduler->poll(m_socket, op, timeout);
    }


    Task<ConnectStatus> Client::connect(std::chrono::milliseconds timeout) {
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
        clientaddr.sin_port = htons(m_remote_endpoint.port);
        clientaddr.sin_addr = *reinterpret_cast<const in_addr*>(m_remote_endpoint.address.data().data());

        auto cret = ::connect(m_socket.fd(), (struct sockaddr*)&clientaddr, sizeof(clientaddr));
        if (cret == 0) {
            co_return return_value(ConnectStatus::Connected);

            // 连接未立即完成（非阻塞模式）
        } else if (cret == -1) {
            if (errno == EAGAIN || errno == EINPROGRESS) {
                // 等待可写事件
                auto pstatus = co_await m_scheduler->poll(m_socket, PollOp::Write, timeout);
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

    Client::Client(std::shared_ptr<IoScheduler> scheduler, Socket socket, IpAddress remote_ip, uint16_t remote_port)
        : m_scheduler(std::move(scheduler)), m_socket(std::move(socket)),
          m_remote_endpoint{remote_ip, remote_port},
          m_connect_status(ConnectStatus::Connected) {}

} // namespace coro::net::tcp