//
// Created by yh on 2/26/25.
//

#ifndef CORO_RECV_STATUS_HPP
#define CORO_RECV_STATUS_HPP

#include <string>
#include <error.h>


enum class RecvStatus : int {
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

static const std::string RecvStatus_Ok {"ok"};
static const std::string RecvStatus_Closed {"closed"};
static const std::string RecvStatus_TryAgain {"try_again"};
static const std::string RecvStatus_PermissionDenied {"permission_denied"};
static const std::string RecvStatus_Interrupted {"interrupted"};
static const std::string RecvStatus_WouldBlock {"would_block"};
static const std::string RecvStatus_BadFileDescriptor {"bad_file_descriptor"};
static const std::string RecvStatus_ConnectionRefused {"connection_refused"};
static const std::string RecvStatus_NotConnected {"not_connected"};
static const std::string RecvStatus_Unknown {"unknown_recv_status"};

const std::string& to_string(const RecvStatus& status) {
    switch (status) {
        case RecvStatus::Ok:
            return RecvStatus_Ok;
        case RecvStatus::Closed:
            return RecvStatus_Closed;
            // case RecvStatus::TryAgain: return RecvStatus_TryAgain;
        case RecvStatus::PermissionDenied:
            return RecvStatus_PermissionDenied;
        case RecvStatus::Interrupted:
            return RecvStatus_Interrupted;
        case RecvStatus::WouldBlock:
            return RecvStatus_WouldBlock;
        case RecvStatus::BadFileDescriptor:
            return RecvStatus_BadFileDescriptor;
        case RecvStatus::ConnectionRefused:
            return RecvStatus_ConnectionRefused;
        case RecvStatus::NotConnected:
            return RecvStatus_NotConnected;
    }
    return RecvStatus_Unknown;
}

#endif //CORO_RECV_STATUS_HPP
