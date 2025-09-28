#include "coro/net/recv_status.hpp"

namespace coro::net {

    static const std::string recv_status_ok{"ok"};
    static const std::string recv_status_closed{"closed"};
    static const std::string recv_status_udp_not_bound{"udp_not_bound"};
    static const std::string recv_status_would_block{"would_block"};
    static const std::string recv_status_connection_refused{"connection_refused"};
    static const std::string recv_status_interrupted{"interrupted"};
    static const std::string recv_status_invalid_argument{"invalid_argument"};
    static const std::string recv_status_no_memory{"out_of_memory"};
    static const std::string recv_status_not_connected{"not_connected"};
    static const std::string recv_status_unknown{"unknown"};


    const std::string &to_string(RecvStatus status) {
        switch (status) {
            case RecvStatus::Ok:
                return recv_status_ok;
            case RecvStatus::Closed:
                return recv_status_closed;
            case RecvStatus::UdpNotBound:
                return recv_status_udp_not_bound;
            case RecvStatus::WouldBlock:
                return recv_status_would_block;
            case RecvStatus::ConnectionRefused:
                return recv_status_connection_refused;
            case RecvStatus::Interrupted:
                return recv_status_interrupted;
            case RecvStatus::OutOfMemory:
                return recv_status_no_memory;
            case RecvStatus::NotConnected:
                return recv_status_not_connected;
            default:
                return recv_status_unknown;
        }
    }

}