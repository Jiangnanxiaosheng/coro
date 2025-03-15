#ifndef CORO_CONNECT_STATUS_HPP
#define CORO_CONNECT_STATUS_HPP

#include <string>

namespace coro::net {

    enum class ConnectStatus {
        Connected, InvalidIpAddress, Timeout, Error,
    };

    const std::string& to_string(const ConnectStatus& status);
}



#endif //CORO_CONNECT_STATUS_HPP
