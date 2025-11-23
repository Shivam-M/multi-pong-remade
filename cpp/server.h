#pragma once

#include "logger.h"

#include <protobufs/pong.pb.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <cstdlib>
#include <optional>

#define NOMINMAX
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


static constexpr int MULTI_PONG_SERVER_PORT = 5001;
static constexpr int MULTI_PONG_SERVER_BUFFER = 512;
static constexpr int MULTI_PONG_SERVER_UPDATE_RATE = 1000 / 128;
static constexpr float MULTI_PONG_BALL_WIDTH = 0.015f;
static constexpr float MULTI_PONG_BALL_HEIGHT = 0.015f * (16.0f / 9.0f);
static constexpr float MULTI_PONG_PADDLE_WIDTH = 0.015f;
static constexpr float MULTI_PONG_PADDLE_HEIGHT = 0.15f;
static constexpr float MULTI_PONG_PADDLE_HORIZONTAL_PADDING = 0.1f;
static constexpr float MULTI_PONG_PADDLE_HIT_EDGE_FACTOR = 0.5f;


class Server {
    private:
        multi_pong::Tokens tokens;
        std::string secret = "";
        float ball_velocity[2] = {0.0f, 0.0f};
        multi_pong::State state;
        multi_pong::Status status;
        std::unordered_map<std::string, multi_pong::Player> clients;
        std::unordered_map<std::string, sockaddr_in> token_addresses;
        int server_socket;

        multi_pong::Tokens generate_tokens();

        void listen();
        void handle_query(const sockaddr_in& address);
        void handle_prepare(const multi_pong::Prepare& prepare, const sockaddr_in& address);
        void handle_join(const multi_pong::Join& join, const sockaddr_in& address);
        void handle_movement(const multi_pong::Movement& movement, const sockaddr_in& address);
        void start_match();
        void game_loop();
        void reset_ball();
        bool did_ball_hit_paddle(float paddle_x, float paddle_y, float& relative_hit);
        void send_state_to_all_players();

        template<typename T>
        void send(const T& data, const sockaddr_in& address);

        std::string get_token_by_player_id(const multi_pong::Player::Identifier& player_id);
        std::optional<multi_pong::Player::Identifier> get_player_id_by_token(const std::string& token);

    public:
        Server() {

#ifdef _WIN32
            WSADATA wsa_data;
            if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
                Logger::error("Failed to initialise Winsock");
                return;
            }
#endif

            tokens = generate_tokens();
            secret = "";

            auto* ball = state.mutable_ball();
            ball->set_x(0.5f);
            ball->set_y(0.5f);
            state.set_frame(0);
            status.set_phase(multi_pong::Status::WAITING);

            server_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (server_socket < 0) {
                Logger::error("Failed to create socket");
                return;
            }
            
            sockaddr_in addr{};
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(MULTI_PONG_SERVER_PORT);
            
            if (bind(server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                Logger::error("Failed to bind socket to port ", MULTI_PONG_SERVER_PORT);
                return;
            }
            
            Logger::info("Socket bound successfully to port ", MULTI_PONG_SERVER_PORT);
            listen();
        }

        ~Server() {
#ifdef _WIN32
            closesocket(server_socket);
            WSACleanup();
#else
            close(server_socket);
#endif
        }
};