//
// Created by JYH on 2025-02-25.
//

#ifndef CORO_CONNECT_STATUS_HPP
#define CORO_CONNECT_STATUS_HPP

#include <string>

enum class ConnectStatus {
    Connected,
    InvalidIpAddress,
    Timeout,
    Error,
};

const static std::string ConnectStatus_Connected {"connected"};
const static std::string ConnectStatus_InvalidIpAddress {"invalid_ip_address"};
const static std::string ConnectStatus_Timeout {"timeout"};
const static std::string ConnectStatus_Error {"error"};
const static std::string ConnectStatus_Unknown {"unknown_connect_status"};

inline const std::string& to_string(const ConnectStatus& status) {
    switch (status) {
        case ConnectStatus::Connected:
            return ConnectStatus_Connected;
        case ConnectStatus::InvalidIpAddress:
            return ConnectStatus_InvalidIpAddress;
        case ConnectStatus::Timeout:
            return ConnectStatus_Timeout;
        case ConnectStatus::Error:
            return ConnectStatus_Error;
    }
    return ConnectStatus_Unknown;
}

#endif //CORO_CONNECT_STATUS_HPP
