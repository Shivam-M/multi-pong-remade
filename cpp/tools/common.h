#pragma once

#include <protobufs/pong.pb.h>
#include <string>
#include <utility>

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

inline constexpr float MULTI_PONG_BALL_WIDTH = 0.015f;
inline constexpr float MULTI_PONG_BALL_HEIGHT = 0.015f * (16.0f / 9.0f);
inline constexpr float MULTI_PONG_PADDLE_WIDTH = 0.015f;
inline constexpr float MULTI_PONG_PADDLE_HEIGHT = 0.15f;
inline constexpr float MULTI_PONG_PADDLE_HORIZONTAL_PADDING = 0.1f;
inline constexpr float MULTI_PONG_PADDLE_HIT_EDGE_FACTOR = 0.5f;

inline constexpr int MULTI_PONG_COORDINATOR_PORT = 4999;
inline constexpr int MULTI_PONG_SERVER_PORT = 5001;
inline constexpr int MULTI_PONG_SERVER_BUFFER = 512;
inline constexpr int MULTI_PONG_SERVER_CHECK_INTERVAL = 5;
inline constexpr int MULTI_PONG_SERVER_CHECK_TIMEOUT = 1;
inline constexpr int MULTI_PONG_SERVER_UPDATE_RATE = 1000 / 128;
inline const std::pair<std::string, int> MULTI_PONG_COORDINATOR_ADDRESS = { "127.0.0.1", 4999 };

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
