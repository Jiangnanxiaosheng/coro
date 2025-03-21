//
// Created by yh on 2/26/25.
//

#include <arpa/inet.h>
#include <array>
#include <span>
#include <iostream>
#include <string>
#include <cstring>

enum class Domain {
    IpV4 = AF_INET,
    IpV6 = AF_INET6,
};

static std::string Domain_Ipv4{"ipv4"};
static std::string Domain_Ipv6{"ipv6"};

const std::string& to_string(Domain domain) {
    switch (domain) {
        case Domain::IpV4:
            return Domain_Ipv4;
        case Domain::IpV6:
            return Domain_Ipv6;
    }
    throw std::runtime_error {"to_string(Domain) unknown domain"};
}


class IpAddress {
public:
    static constexpr size_t ipv4_len {4};
    static constexpr size_t ipv6_len {16};

    IpAddress() = default;
    IpAddress(std::span<const uint8_t> binary_address, Domain domain = Domain::IpV4) : m_domain(domain) {
        if (m_domain == Domain::IpV4 && binary_address.size() > ipv4_len) {
            throw std::runtime_error{"coro::net::IpAddress provided binary ip address is too long"};
        } else if (binary_address.size() > ipv6_len) {
            throw std::runtime_error{"coro::net::IpAddress provided binary ip address is too long"};
        }
        std::copy(binary_address.begin(), binary_address.end(), m_data.begin());
    }

    Domain domain() const {
        return m_domain;
    }

    static auto from_string(const std::string& address, Domain domain = Domain::IpV4) {
        IpAddress addr {};
        addr.m_domain = domain;

        auto success = inet_pton(static_cast<int>(addr.m_domain), address.data(), addr.m_data.data());
        if (success != 1) {
            throw std::runtime_error {"coro::net::IpAdress faild to convert from string"};
        }
        return addr;
    }

    std::string to_string() const {
        std::string output;
        if (m_domain == Domain::IpV4) {
            output.resize(INET_ADDRSTRLEN, '\0');
        } else {
            output.resize(INET6_ADDRSTRLEN, '\0');
        }

        auto success = inet_ntop(static_cast<int>(m_domain), m_data.data(), output.data(), output.length());
        if (success != nullptr) {
            auto len = strnlen(success, output.length());
            output.resize(len);
        } else {
            throw std::runtime_error {"coro::net::IpAddress failed to convert to string representation"};
        }

        return output;
    }

    bool operator==(const IpAddress& other) const = default;
    //auto operator<=>(const IpAddress& other) const = default;

private:
    std::array<uint8_t, ipv6_len> m_data{};
    Domain m_domain {Domain::IpV4};
};

int main() {
    auto ip_addr1 = IpAddress::from_string("127.0.0.1");
    auto ip_addr2 = IpAddress(std::array<uint8_t, 4>{196, 168, 0, 1});
    uint8_t arr[] = {127,0,0,1};
    auto ip_addr3 = IpAddress(arr);



    std::cout << ip_addr1.to_string() << "\n";
    std::cout << ip_addr2.to_string() << "\n";
    ip_addr1 == ip_addr3;
    std::cout << std::boolalpha <<  (ip_addr1 == ip_addr3);

    return 0;
}