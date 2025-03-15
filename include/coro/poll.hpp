#ifndef CORO_POLL_HPP
#define CORO_POLL_HPP

#include <sys/epoll.h>
#include <string>

namespace coro {
    enum class PollOp {
        Read = EPOLLIN,
        Write = EPOLLOUT,
        ReadWrite = EPOLLIN | EPOLLOUT,
    };

    inline bool poll_op_readable(PollOp op) {
        return (static_cast<int>(op) & EPOLLIN);
    }

    inline bool poll_op_writeable(PollOp op) {
        return (static_cast<int>(op) & EPOLLOUT);
    }

    const std::string& to_string(PollOp status);

    enum class PollStatus {
        Event,  // epoll operation was successful
        Timeout,
        Error,
        Closed,
    };

    const std::string& to_string(PollStatus status);
}

#endif //CORO_POLL_HPP
