#include "client.h"
#include "tools/logger.h"
#include "tools/renderer_opengl.h"

#include <thread>

using namespace multi_pong;

Client::Client() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        Logger::error("Failed to initialise Winsock");
        return;
    }
#endif

    coordinator_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (coordinator_socket < 0) {
        Logger::error("Failed to create coordinator socket");
        return;
    }

    if (!connect_coordinator()) {
        return;
    }

    std::thread listen_coordinator_thread(&Client::listen_coordinator, this);
    listen_coordinator_thread.detach();

    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        Logger::error("Failed to create server socket");
        return;
    }

    Message search_message = Message();
    search_message.mutable_search()->CopyFrom(Search());
    send_message_to_coordinator(search_message);

    renderer = std::make_unique<OpenGLRenderer>(this);
    renderer->setup();

    update_loop();
}

Client::~Client() {
    close_socket(coordinator_socket);
    close_socket(server_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    active = false;
}

bool Client::connect_coordinator() {
    sockaddr_in coordinator_server;
    coordinator_server.sin_family = AF_INET;
    coordinator_server.sin_port = htons(MULTI_PONG_COORDINATOR_ADDRESS.second);
    inet_pton(AF_INET, MULTI_PONG_COORDINATOR_ADDRESS.first.c_str(), &coordinator_server.sin_addr);

    if (connect(coordinator_socket, (struct sockaddr*)&coordinator_server, sizeof(coordinator_server)) < 0) {
        Logger::error("Failed to connect to the game coordinator");
        return false;
    }
    
    Logger::info("Connected to game coordinator");
    return true;
}

void Client::listen_coordinator() {
    Logger::info("Listening the game coordinator");
    while (active) {
        char buffer[MULTI_PONG_SERVER_BUFFER];

        int receive_bytes = recv(coordinator_socket, buffer, MULTI_PONG_SERVER_BUFFER, 0);

        if (!receive_bytes) continue;

        Message message;
        if (!message.ParseFromArray(buffer, receive_bytes)) {
            Logger::warning("Failed to process data into a protobuf message: ", buffer);
            continue;
        }

        switch (message.content_case()) {
            case Message::kMatch:
                handle_match(message.match());
                break;
        }
    }
}

void Client::listen_server() {
    Logger::info("Listening the game server");
    char buffer[MULTI_PONG_SERVER_BUFFER];
    sockaddr_in source_address{};
    socklen_t source_address_len = sizeof(source_address);

    while (active) {
        int received = recvfrom(server_socket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&source_address, &source_address_len);

        if (received < 0) continue;

        buffer[received] = '\0';

        Message received_message;
        if (!received_message.ParseFromArray(buffer, received)) {
            Logger::warning("Failed to process data into a protobuf message: ", buffer);
            continue;
        }

        switch (received_message.content_case()) {
            case Message::kState:
                state = received_message.state();
                break;
            default:
                Logger::warning("Invalid message type ", received_message.content_case(), " from server");
                break;
            };
    }
}

void Client::send_message_to_coordinator(multi_pong::Message message) {
    std::string serialised_message = message.SerializeAsString();
    send(coordinator_socket, serialised_message.c_str(), serialised_message.size(), 0);
}

template<typename T>
void Client::send_message_to_server(const T& data) {
    Message message;

    if constexpr (std::is_same_v<T, Join>) {
        message.mutable_join()->CopyFrom(data);
    } else if constexpr (std::is_same_v<T, Movement>) {
        message.mutable_movement()->CopyFrom(data);
    } else {
        return;
    }

    std::string serialised_message;
    message.SerializeToString(&serialised_message);
    sendto(server_socket, serialised_message.data(), serialised_message.size(), 0, (struct sockaddr*)&server_address, sizeof(server_address));
}

template void Client::send_message_to_server<Join>(const Join&);
template void Client::send_message_to_server<Movement>(const Movement&);

void Client::handle_match(multi_pong::Match match) {
    identifier = match.player().identifier();
    token = match.token();

    Join join = Join();
    join.set_token(token);

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, match.host().c_str(), &server_address.sin_addr);
    server_address.sin_port = htons(match.port());
    
    send_message_to_server(join);

    std::thread listen_server_thread(&Client::listen_server, this);
    listen_server_thread.detach();
}

void Client::send_move(multi_pong::Direction move) {
    Movement movement = Movement();
    movement.set_token(token);
    movement.set_direction(move);
    send_message_to_server(movement);
}

void Client::update_loop() {
    renderer->render_loop();
}

// todo: move to a single main.cpp which takes --server, --coordinator and --client as command line args with conditional cmake builds
int main() {
    Client client;
    return 0;
}
