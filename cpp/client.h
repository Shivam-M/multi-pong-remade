#pragma once

#include "tools/common.h"
#include "tools/renderer.h"

#include <string>
#include <utility>
#include <memory>


class Client {
    private:
        std::unique_ptr<Renderer> renderer;

        std::pair<std::string, int> coordinator_address;
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
        Client(const std::string& address, int port, std::unique_ptr<Renderer> game_renderer);
        ~Client();

        void send_move(multi_pong::Direction move);
        const multi_pong::State& get_state() { return state; }
};
