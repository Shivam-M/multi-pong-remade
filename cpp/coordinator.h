#pragma once

#include "logger.h"

#include <protobufs/pong.pb.h>
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <optional>
#include <utility>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif


static constexpr int MULTI_PONG_COORDINATOR_PORT = 4999;
static constexpr int MULTI_PONG_SERVER_CHECK_INTERVAL = 5;
static constexpr int MULTI_PONG_SERVER_CHECK_TIMEOUT = 1;
static constexpr int MULTI_PONG_SERVER_BUFFER = 512;


class Coordinator {
    private:
        int coordinator_socket;
        std::string secret = "";
        std::vector<int> clients;
        std::deque<int> searching_clients;
        std::map<std::pair<std::string, int>, int> server_list = {
            { {"127.0.0.1", 5000}, -1 },
            { {"127.0.0.1", 5001}, -1 },
            { {"127.0.0.1", 5002}, -1 },
            { {"127.0.0.1", 5003}, -1 },
            { {"127.0.0.1", 5004}, -1 },
        };

        void listen_clients();
        void check_status();
        void matchmake();
        bool get_prepared_server(std::pair<std::string, int>& server, multi_pong::Tokens& tokens);
        void send_message_to_client(int client, const multi_pong::Message&);
        std::optional<multi_pong::Message> send_message_to_server(std::pair<std::string, int> server, multi_pong::Message message);

    public:
        Coordinator() {
            std::thread status_thread(&Coordinator::check_status, this);
            status_thread.detach();

    #ifdef _WIN32
            WSADATA wsa_data;
            if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
                Logger::error("Failed to initialise Winsock");
                return;
            }
    #endif

            coordinator_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (coordinator_socket < 0) {
                Logger::error("Failed to create socket");
                return;
            }

            sockaddr_in addr{};
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(MULTI_PONG_COORDINATOR_PORT);

            int enable = 1;
#ifdef _WIN32
            setsockopt(coordinator_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable));
            setsockopt(coordinator_socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&enable, sizeof(enable));
#else
            setsockopt(coordinator_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif

            int bound = bind(coordinator_socket, (struct sockaddr*)&addr, sizeof(addr));
            if (bound < 0) {
                Logger::error("Failed to bind socket to port ", MULTI_PONG_COORDINATOR_PORT, ": ", bound);
                return;
            }

            Logger::debug("Socket bound successfully to port ", MULTI_PONG_COORDINATOR_PORT);
            listen_clients();
        }

        ~Coordinator() {
    #ifdef _WIN32
            closesocket(coordinator_socket);
            WSACleanup();
    #else
            close(coordinator_socket);
    #endif
        }
};