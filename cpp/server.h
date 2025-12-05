#pragma once

#include "tools/common.h"

#include <string>
#include <unordered_map>
#include <optional>


class Server {
    private:
        int port;
        multi_pong::Tokens tokens;
        std::string secret = "";
        float ball_velocity[2] = {0.0f, 0.0f};
        multi_pong::State state;
        multi_pong::Status status;
        std::unordered_map<std::string, multi_pong::Player> clients;
        std::unordered_map<std::string, sockaddr_in> token_addresses;
        socket_t server_socket;

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
        Server(int port);
        ~Server();
};