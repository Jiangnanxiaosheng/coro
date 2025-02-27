//
// Created by yh on 2/26/25.
//

#ifndef CORO_IP_ADDRESS_HPP
#define CORO_IP_ADDRESS_HPP


#include <arpa/inet.h>
#include <array>
#include <span>
#include <string>
#include <cstring>
#include <stdexcept>


class IpAddress {
public:
    static constexpr size_t ipv4_len {4};

    IpAddress() = default;
    IpAddress(std::span<const uint8_t> binary_address) {
        if (binary_address.size() > ipv4_len) {
            throw std::runtime_error{"coro::net::IpAddress provided binary ipv4 address is too long"};
        }
        std::copy(binary_address.begin(), binary_address.end(), m_data.begin());
    }


    static auto from_string(const std::string& ip) {
        IpAddress addr {};

        auto success = inet_pton(AF_INET, ip.data(), addr.m_data.data());
        if (success != 1) {
            throw std::runtime_error {"coro::net::IpAddress failed to convert from string"};
        }
        return addr;
    }

    std::string to_string() const {
        std::string output;
        output.resize(INET_ADDRSTRLEN, '\0');

        auto success = inet_ntop(AF_INET, m_data.data(), output.data(), output.length());
        if (success != nullptr) {
            auto len = strnlen(success, output.length());
            output.resize(len);
        } else {
            throw std::runtime_error {"coro::net::IpAddress failed to convert to string representation"};
        }

        return output;
    }

    std::span<const uint8_t> data() const {
        return std::span<const uint8_t>{m_data.data(), ipv4_len};
    }

private:
    std::array<uint8_t, ipv4_len> m_data{};
};

#endif //CORO_IP_ADDRESS_HPP
