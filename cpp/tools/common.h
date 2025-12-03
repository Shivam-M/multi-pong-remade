#pragma once

#include <protobufs/pong.pb.h>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif

#ifdef _WIN32
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

inline void close_socket(socket_t socket_) {
#ifdef _WIN32
    closesocket(socket_);
#else
    close(socket_);
#endif
}

inline std::string address_string(const sockaddr_in& addr) {
    char address_buffer[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr.sin_addr, address_buffer, INET_ADDRSTRLEN) == nullptr) {
        return "unknown";
    }
    return std::string(address_buffer);
}
