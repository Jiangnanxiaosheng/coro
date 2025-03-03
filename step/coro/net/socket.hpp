//
// Created by yh on 2/27/25.
//

#ifndef CORO_SOCKET_HPP
#define CORO_SOCKET_HPP

#include <arpa/inet.h>
#include <utility>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

#include "ip_address.hpp"
#include "poll.hpp"


class Socket {
public:
    enum class Type : uint16_t {
        Tcp = SOCK_STREAM,
        Udp = SOCK_DGRAM,
    };

    Socket() = default;
    explicit Socket(int fd) : m_fd(fd) {}

    Socket(const Socket& other) : m_fd(dup(other.m_fd)) {}
    Socket& operator=(const Socket& other) {
        if (std::addressof(other) != this) {
            this->close();
            this->m_fd = dup(other.m_fd);
        }
        return *this;
    }

    Socket(Socket&& other)  noexcept : m_fd(std::exchange(other.m_fd, -1)) {}
    Socket& operator=(Socket&& other) noexcept {
        if (std::addressof(other) != this) {
            this->close();
            this->m_fd = std::exchange(other.m_fd, -1);
        }
        return *this;
    }

    ~Socket() { close(); }

public:
    bool set_blocking(bool blocking) {
        if (m_fd < 0) {
            return false;
        }

        int flags = fcntl(m_fd, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }

        flags = (blocking) ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);

        return (fcntl(m_fd, F_SETFL, flags) == 0);
    }

    bool is_valid() const { return m_fd != -1; }
    int fd() const { return m_fd; }

    void close() {
        if (m_fd != -1) {
            ::close(m_fd);
            m_fd = -1;
        }
    }

    bool shutdown(PollOp op) {
        if (m_fd != -1) {
            int h{0};
            switch (op) {
                case PollOp::Read:
                    h = SHUT_RD;
                    break;
                case PollOp::Write:
                    h = SHUT_WR;
                    break;
                case PollOp::ReadWrite:
                    h = SHUT_RDWR;
                    break;
            }

            return (::shutdown(m_fd, h) == 0);
        }
        return false;
    }

private:
    int m_fd {-1};
};


inline Socket make_nonblock_socket(Socket::Type type = Socket::Type::Tcp) {
    Socket sock{::socket(AF_INET, static_cast<uint16_t>(type) | SOCK_NONBLOCK, 0)};
    if (sock.fd() < 0) {
        throw std::runtime_error{"failed to create nonblock socket."};
    }

    return sock;
}

inline Socket make_accept_socket(const IpAddress& address, uint16_t port, int backlog = 128, Socket::Type type = Socket::Type::Tcp) {
    Socket sock = make_nonblock_socket(type);

    int opt {1};
    if (setsockopt(sock.fd(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error{"failed to setsockopt(SO_REUSEADDR | SO_REUSEPORT)."};
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr = *reinterpret_cast<const in_addr*>(address.data().data());

    if (bind(sock.fd(), (struct sockaddr*)&server, sizeof(server)) < 0) {
        throw std::runtime_error{"failed to bind"};
    }

    if (type  == Socket::Type::Tcp) {
        if (listen(sock.fd(), backlog) < 0) {
            throw std::runtime_error{"failed to listen"};
        }
    }

    return sock;
}




#endif //CORO_SOCKET_HPP
