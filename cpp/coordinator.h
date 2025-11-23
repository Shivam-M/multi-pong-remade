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
        Coordinator();
        ~Coordinator();
};