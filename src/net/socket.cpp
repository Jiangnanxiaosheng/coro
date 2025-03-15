#include "coro/net/socket.hpp"

namespace coro::net {

    Socket::Socket(int fd) : m_fd(fd) {}

    // 文件描述符需通过dup()复制，确保每个对象管理独立的资源
    Socket::Socket(const Socket &other) : m_fd(dup(other.m_fd)) {}

    Socket &Socket::operator=(const Socket &other) {
        if (std::addressof(other) != this) {
            this->close();
            m_fd = dup(other.m_fd);
        }
        return *this;
    }

    // 直接转移资源所有权，原对象置为无效
    Socket::Socket(Socket &&other) noexcept: m_fd(std::exchange(other.m_fd, -1)) {}

    Socket &Socket::operator=(Socket &&other) noexcept {
        if (std::addressof(other) != this) {
            this->close();
            m_fd = std::exchange(other.m_fd, -1);
        }
        return *this;
    }


    int Socket::fd() const {
        return m_fd;
    }

    bool Socket::is_valid() {
        return m_fd != -1;
    }

    void Socket::close() {
        if (m_fd != -1) {
            ::close(m_fd);
            m_fd = -1;
        }
    }


    Socket make_nonblocking_socket(SocketType type) {
        Socket sock{::socket(AF_INET, static_cast<short>(type) | SOCK_NONBLOCK, 0)};
        if (sock.fd() < 0) {
            throw std::runtime_error{"Failed to create nonblocking socket."};
        }

        return sock;
    }

    Socket make_accept_socket(const IpAddress &ip, uint16_t port, SocketType type) {
        return make_accept_socket(ip, port, 128, type);
    }

    Socket make_accept_socket(const IpAddress &ip, uint16_t port, int backlog, SocketType type) {
        Socket sock = make_nonblocking_socket(type);

        int opt{1};
        if (setsockopt(sock.fd(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error{"Failed to setsockopt(SO_REUSEADDR | SO_REUSEPORT)"};
        }

        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr = *reinterpret_cast<const in_addr *>(ip.data().data());

        if (::bind(sock.fd(), (sockaddr*) &server, sizeof(server)) < 0) {
            throw std::runtime_error{"Failed to bind."};
        }

        if (type == SocketType::Tcp) {
            if (listen(sock.fd(), backlog) < 0) {
                throw std::runtime_error{"Failed to listen."};
            }
        }

        return sock;
    }

} // namespace coro::net
