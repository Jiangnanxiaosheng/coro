#include "coro/net/connect_status.hpp"


namespace coro::net {

    const static std::string connect_status_connected{"connected"};
    const static std::string connect_status_invalid_ip_address{"invalid_ip_address"};
    const static std::string connect_status_timeout{"timeout"};
    const static std::string connect_status_error{"error"};


    const std::string& to_string(const ConnectStatus& status) {
        switch (status) {
            case ConnectStatus::Connected:
                return connect_status_connected;
            case ConnectStatus::InvalidIpAddress:
                return connect_status_invalid_ip_address;
            case ConnectStatus::Timeout:
                return connect_status_timeout;
            case ConnectStatus::Error:
                [[fallthrough]];
            default:
                return connect_status_error;
        }
    }
}

