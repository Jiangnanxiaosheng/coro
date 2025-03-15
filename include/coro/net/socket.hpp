#ifndef CORO_SOCKET_HPP
#define CORO_SOCKET_HPP

#include "coro/net/ip_address.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <stdexcept>

namespace coro::net {

    enum class SocketType : short {
        Tcp = SOCK_STREAM,
        Udp = SOCK_DGRAM,
    };

    class Socket {
    public:
        Socket() = default;
        explicit Socket(int fd);

        Socket(const Socket &other);

        Socket &operator=(const Socket &other);

        Socket(Socket &&other) noexcept;

        Socket &operator=(Socket &&other) noexcept;

        ~Socket() { close(); }

        int fd() const;

        bool is_valid();

        void close();

    private:
        int m_fd{-1};
    };

    Socket make_nonblocking_socket(SocketType type = SocketType::Tcp);

    Socket make_accept_socket(const IpAddress &ip, uint16_t port, SocketType type = SocketType::Tcp);

    Socket make_accept_socket(const IpAddress &ip, uint16_t port, int backlog = 128, SocketType type = SocketType::Tcp);

} // namespace coro::net

#endif //CORO_SOCKET_HPP
