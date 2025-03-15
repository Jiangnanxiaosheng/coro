#ifndef CORO_IP_ADDRESS_HPP
#define CORO_IP_ADDRESS_HPP

#include <span>
#include <array>
#include <arpa/inet.h>
#include <string>
#include <cstring>
#include <stdint.h>
#include <stdexcept>

namespace coro::net {

    class IpAddress {
    public:
        static constexpr size_t ipv4_len{4};

        IpAddress() = default;

        // std::span<const uint8_t>允许接受临时对象
        IpAddress(std::span<const uint8_t> binary_address);

        static IpAddress from_string(const std::string &ip_str);

        std::span<const uint8_t> data() const;

        std::string to_string() const;

        // 实现了==重载，c++20编译器自动实现对 != 支持
        bool operator==(const IpAddress& other) const = default;

    private:
        std::array<uint8_t, ipv4_len> m_ip;
    };

}
#endif //CORO_IP_ADDRESS_HPP
