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

static void close_socket(socket_t socket_) {
#ifdef _WIN32
    closesocket(socket_);
#else
    close(socket_);
#endif
}
