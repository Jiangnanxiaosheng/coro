//
// Created by yh on 2/27/25.
//

#ifndef CORO_POLL_HPP
#define CORO_POLL_HPP

#include <sys/epoll.h>
#include <string>

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


inline const std::string& to_string(PollOp op) {
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

inline const std::string& to_string(PollStatus status) {
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

#endif //CORO_POLL_HPP
