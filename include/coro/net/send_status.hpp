#ifndef CORO_SEND_STATUS_HPP
#define CORO_SEND_STATUS_HPP

#include <errno.h>

namespace coro::net {
    enum class SendStatus {
        Ok = 0,
        Closed = -1,
        TryAgain = EAGAIN,
        PermissionDenied = EACCES,
        Interrupted = EINTR,
        WouldBlock = EWOULDBLOCK,
        BadFileDescriptor = EBADF,
        ConnectionRefused = ECONNREFUSED,
        NotConnected = ENOTCONN,
    };
}

#endif //CORO_SEND_STATUS_HPP
