#pragma once

#include "tools/common.h"
#include "tools/renderer.h"

#include <string>
#include <utility>
#include <memory>


static const std::pair<std::string, int> MULTI_PONG_COORDINATOR_ADDRESS = { "127.0.0.1", 4999 };
static constexpr int MULTI_PONG_SERVER_BUFFER = 512;
static constexpr float MULTI_PONG_BALL_WIDTH = 0.015f;
static constexpr float MULTI_PONG_BALL_HEIGHT = 0.015f * (16.0f / 9.0f);
static constexpr float MULTI_PONG_PADDLE_WIDTH = 0.015f;
static constexpr float MULTI_PONG_PADDLE_HEIGHT = 0.15f;
static constexpr float MULTI_PONG_PADDLE_HORIZONTAL_PADDING = 0.1f;


class Client {
    private:
        std::unique_ptr<Renderer> renderer;

        socket_t coordinator_socket;
        socket_t server_socket;
        sockaddr_in server_address;
        multi_pong::State state;
        std::string token;
        int identifier = 0;

        std::atomic<bool> active{ true };

        bool connect_coordinator();
        void listen_coordinator();
        void listen_server();
        void send_message_to_coordinator(multi_pong::Message message);
        
        template<typename T>
        void send_message_to_server(const T& data);
        
        void handle_match(multi_pong::Match match);
        void update_loop();

    public:
        Client();
        ~Client();

        void send_move(multi_pong::Direction move);
        const multi_pong::State& get_state() { return state; }
};
