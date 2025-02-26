//
// Created by yh on 2/26/25.
//

#include "coro/net/ip_address.hpp"

#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

class Socket {
public:
    enum class Type { Udp, Tcp, };
    enum class Blocking { Yes, No, };

    struct Options {
        Domain domain {Domain::Ipv4};
    };

private:
    int m_fd {-1};
};

Socket make_socket() {

}

Socket make_accept_socket() {

}


int main() {


    return 0;
}