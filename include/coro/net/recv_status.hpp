#ifndef CORO_RECV_STATUS_HPP
#define CORO_RECV_STATUS_HPP

#include <errno.h>
#include <string>

namespace coro::net {
    enum class RecvStatus : long {
        Ok = 0,
        Closed = -1,
        UdpNotBound = -2,
        TryAgain = EAGAIN,
        WouldBlock = EWOULDBLOCK,
        ConnectionRefused = ECONNREFUSED,
        Interrupted = EINTR,
        OutOfMemory = ENOMEM,
        NotConnected = ENOTCONN
    };

    const std::string& to_string(RecvStatus status);
}

#endif //CORO_RECV_STATUS_HPP
