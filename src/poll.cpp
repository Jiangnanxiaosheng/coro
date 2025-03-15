#include "coro/poll.hpp"

namespace coro {

    static const std::string poll_unknown{"unknown"};

    static const std::string poll_op_read{"read"};
    static const std::string poll_op_write{"write"};
    static const std::string poll_op_read_write{"read_write"};

    const std::string& to_string(PollOp op) {
        switch (op) {
            case PollOp::Read:
                return poll_op_read;
            case PollOp::Write:
                return poll_op_write;
            case PollOp::ReadWrite:
                return poll_op_read_write;
            default:
                return poll_unknown;
        }
    }

    static const std::string poll_status_event{"event"};
    static const std::string poll_status_timeout{"timeout"};
    static const std::string poll_status_error{"error"};
    static const std::string poll_status_closed{"closed"};

    const std::string& to_string(PollStatus status) {
        switch (status) {
            case PollStatus::Event:
                return poll_status_event;
            case PollStatus::Timeout:
                return poll_status_timeout;
            case PollStatus::Error:
                return poll_status_error;
            case PollStatus::Closed:
                return poll_status_closed;
            default:
                return poll_unknown;
        }
    }

} // namespace coro

