//
// Created by yh on 2/26/25.
//

#include "coro/net/ip_address.hpp"

#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <utility>
#include <unistd.h>
#include <sys/epoll.h>

enum class PollOp {
    Read = EPOLLIN,
    Write = EPOLLOUT,
    ReadWrite = EPOLLIN | EPOLLOUT,
};

bool poll_op_readable(PollOp op) {
    return (static_cast<int>(op) & EPOLLIN);
}

bool poll_op_writeable(PollOp op) {
    return (static_cast<int>(op) & EPOLLOUT);
}

static const std::string PollOp_Read{"read"};
static const std::string PollOp_Write{"write"};
static const std::string PollOp_ReadWrite{"read_write"};


const std::string& to_string(PollOp op) {
    switch (op) {
        case PollOp::Read:
            return PollOp_Read;
        case PollOp::Write:
            return PollOp_Write;
        case PollOp::ReadWrite:
            return PollOp_ReadWrite;
    }
}

enum class PollStatus {
    Event, Timeout, Error, Closed,
};

static const std::string PollStatus_Event{"event"};
static const std::string PollStatus_Timeout{"timeout"};
static const std::string PollStatus_Error{"error"};
static const std::string PollStatus_Closed{"closed"};

const std::string& to_string(PollStatus status) {
    switch (status) {
        case PollStatus::Event:
            return PollStatus_Event;
        case PollStatus::Timeout:
            return PollStatus_Timeout;
        case PollStatus::Error:
            return PollStatus_Error;
        case PollStatus::Closed:
            return PollStatus_Closed;
    }
}

class Socket {
public:
    enum class Type : uint16_t {
        Udp = SOCK_DGRAM,
        Tcp = SOCK_STREAM,
    };
    enum class Blocking { Yes, No, };

    struct Options {
        Type type {Type::Tcp};
        Blocking blocking {Blocking::Yes};
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
    bool set_blocking(Blocking blocking) {
        if (m_fd < 0) {
            return false;
        }

        int flags = fcntl(m_fd, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }

        flags = (blocking == Blocking::Yes) ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);

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

Socket make_socket(const Socket::Options& options) {
    Socket sock{::socket(AF_INET, static_cast<uint16_t>(options.type), 0)};
    if (sock.fd() < 0) {
        throw std::runtime_error{"failed to create socket."};
    }

    if (options.blocking == Socket::Blocking::No) {
        if (sock.set_blocking(Socket::Blocking::No) == false) {
            throw std::runtime_error{"failed to set socket to non-blocking mode."};
        }
    }

    return sock;
}

Socket make_accept_socket(const Socket::Options& options, const IpAddress& address, uint16_t port, int backlog = 128) {
    Socket sock = make_socket(options);

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

    if (options.type  == Socket::Type::Tcp) {
        if (listen(sock.fd(), backlog) < 0) {
            throw std::runtime_error{"failed to listen"};
        }
    }

    return sock;
}


int main() {
    IpAddress addr = IpAddress::from_string("127.0.0.1");

    auto sock = make_accept_socket(Socket::Options {},addr, 0, 5);

    return 0;
}