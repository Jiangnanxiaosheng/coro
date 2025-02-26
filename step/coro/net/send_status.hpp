//
// Created by yh on 2/26/25.
//

#ifndef CORO_SEND_STATUS_HPP
#define CORO_SEND_STATUS_HPP

#include <string>
#include <error.h>

enum class SendStatus : int {
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

#endif //CORO_SEND_STATUS_HPP
