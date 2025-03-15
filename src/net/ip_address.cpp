#include "coro/net/ip_address.hpp"


namespace coro::net {

    IpAddress::IpAddress(std::span<const uint8_t> binary_address) {
        if (binary_address.size() > ipv4_len) {
            throw std::runtime_error{"coro::net::IpAddress provide binary ip address is too long"};
        }
        std::copy(binary_address.begin(), binary_address.end(), m_ip.begin());
    }

     IpAddress IpAddress::from_string(const std::string &ip_str) {
        IpAddress addr{};

        auto success = inet_pton(AF_INET, ip_str.data(), addr.m_ip.data());
        if (success != 1) {
            throw std::runtime_error{"coro::net::IpAddress failed to convert from string"};
        }

        return addr;
    }

    std::span<const uint8_t> IpAddress::data() const {
        return std::span<const uint8_t>{m_ip.data(), m_ip.size()};
    }

    std::string IpAddress::to_string() const {
        std::string buffer(INET_ADDRSTRLEN, '\0');

        auto success = inet_ntop(AF_INET, m_ip.data(), buffer.data(), buffer.size());
        if (success != nullptr) {
            auto len = strnlen(success, buffer.size());
            buffer.resize(len);
        } else {
            throw std::runtime_error{"coro::net::IpAddress failed to convert to string representation"};
        }

        return buffer;
    }

} // namespace coro::net