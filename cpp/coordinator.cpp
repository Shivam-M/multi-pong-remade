#include "coordinator.h"
#include "tools/logger.h"

#include <thread>

using namespace multi_pong;

Coordinator::Coordinator() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        Logger::error("Failed to initialise Winsock");
        return;
    }
#endif

    std::thread status_thread(&Coordinator::check_status, this);
    status_thread.detach();

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

Coordinator::~Coordinator() {
    close_socket(coordinator_socket);
#ifdef _WIN32
    WSACleanup();
#endif
}

void Coordinator::check_status() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(MULTI_PONG_SERVER_CHECK_INTERVAL));

		for (const auto& [server, status] : server_list) {
            Message message;
			message.mutable_query()->CopyFrom(Query());
            send_message_to_server(server, message);
        }

        matchmake();
    }
}

void Coordinator::matchmake() {
    if (searching_clients.size() < 2) {
        return;
    }

	std::pair<std::string, int> server;
	Tokens tokens;

    if (!get_prepared_server(server, tokens)) {
        return;
    }

    std::vector<std::pair<std::string, Player::Identifier>> token_pairs = {
        { tokens.token_1(), Player::Identifier::Player_Identifier_PLAYER_1 },
        { tokens.token_2(), Player::Identifier::Player_Identifier_PLAYER_2 }
    };

    for (auto& [token, player_id] : token_pairs) {
        Player player;
        player.set_identifier(player_id);
        player.set_paddle_direction(Direction::STOP);
        player.set_paddle_location(0.5f);
        player.set_score(0);

        Match match;
        match.set_host(server.first);
        match.set_port(server.second);
        match.set_token(token);
        match.mutable_player()->CopyFrom(player);

        socket_t client = searching_clients.front();
		searching_clients.pop_front();

		Message match_message = Message();
		match_message.mutable_match()->CopyFrom(match);

        send_message_to_client(client, match_message);

        Logger::info("Forwarded match on ", server.first, ":", server.second, " to client with token ", token);
    }
}

bool Coordinator::get_prepared_server(std::pair<std::string, int>& prepared_server, Tokens& tokens) {
    Message prepare_message = Message();
    Prepare prepare;

	prepare.set_secret(secret);
    prepare_message.mutable_prepare()->CopyFrom(prepare);

    for (const auto& [server, status] : server_list) {
        if (status != Status::Phase::Status_Phase_WAITING) {
            continue;
        }

        auto token_message = send_message_to_server(server, prepare_message);
        if (token_message && token_message->has_tokens()) {
            prepared_server = server;
			tokens = token_message->tokens();
            return true;
        }
    }
    return false;
}

std::optional<Message> Coordinator::send_message_to_server(std::pair<std::string, int> server, Message message) {
    socket_t server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        return std::nullopt;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(server.second);
    inet_pton(AF_INET, server.first.c_str(), &address.sin_addr);

    int sent = sendto(server_socket, message.SerializeAsString().c_str(), message.ByteSizeLong(), 0, (sockaddr*)&address, sizeof(address));
    if (sent < 0) {
        Logger::error("Failed to send message to server ", server.first, ":", server.second);
        close_socket(server_socket);
        return std::nullopt;
    }

    char buffer[MULTI_PONG_SERVER_BUFFER];
    sockaddr_in source_address{};
    socklen_t source_address_len = sizeof(source_address);

#ifdef _WIN32
    constexpr DWORD timeout = MULTI_PONG_SERVER_CHECK_TIMEOUT * 1000;
#else
    struct timeval timeout;
    timeout.tv_sec = MULTI_PONG_SERVER_CHECK_TIMEOUT;
    timeout.tv_usec = 0;
#endif

    setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    int received = recvfrom(server_socket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&source_address, &source_address_len);

    close_socket(server_socket);

    if (received < 0) {
        Logger::info("Server ", server.first, ":", server.second, " is unresponsive");
        return std::nullopt;
    }

    buffer[received] = '\0';

    Message received_message;
    if (!received_message.ParseFromArray(buffer, received)) {
        Logger::warning("Failed to process data into a protobuf message: ", buffer);
        return std::nullopt;
    }

    switch (received_message.content_case()) {
        case Message::kStatus:
            if (received_message.status().phase() == Status::Phase::Status_Phase_WAITING) {
                Logger::info("Server ", server.first, ":", server.second, " is available");
            }
            else {
                Logger::info("Server ", server.first, ":", server.second, " is busy");
            }
            server_list[server] = received_message.status().phase();
            return received_message;
        case Message::kTokens:
            server_list[server] = Status::Phase::Status_Phase_STARTED;
            return received_message;
        default:
            Logger::warning("Invalid message type ", received_message.content_case(), " from server ", server.first, ":", server.second);
            return received_message;
        };
}

void Coordinator::listen_clients() {
    listen(coordinator_socket, SOMAXCONN);
    Logger::info("Starting listening on 0.0.0.0:", MULTI_PONG_COORDINATOR_PORT);

    while (true) {
		fd_set read_fds;
		FD_ZERO(&read_fds);

		FD_SET(coordinator_socket, &read_fds);
        socket_t max_fd = coordinator_socket;

        for (auto client : clients) {
            FD_SET(client, &read_fds);
			if (client > max_fd) {
                max_fd = client;
            }
        }

        if (select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr) < 0) {
            continue;
		}

        if (FD_ISSET(coordinator_socket, &read_fds)) {
            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);

#ifdef _WIN32
            SOCKET client_socket = accept(coordinator_socket, (sockaddr*)&client_addr, &len);
            if (client_socket == INVALID_SOCKET) {
                Logger::warning("Failed to accept client connection: ", WSAGetLastError());
                continue;
            }
#else
            socket_t client_socket = accept(coordinator_socket, (sockaddr*)&client_addr, &len);
            if (client_socket < 0) {
                Logger::warning("Failed to accept client connection: ", errno);
                continue;
            }
#endif

            clients.push_back(client_socket);

            Logger::info("Client ", inet_ntoa(client_addr.sin_addr), ":", ntohs(client_addr.sin_port), " connected");
        }

        for (auto it = clients.begin(); it != clients.end();) {
            socket_t client_socket = *it;

            if (!FD_ISSET(client_socket, &read_fds)) {
                ++it;
                continue;
            }

            char buffer[MULTI_PONG_SERVER_BUFFER];
            int bytes = recv(client_socket, buffer, sizeof(buffer), 0);

            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            getpeername(client_socket, (sockaddr*)&client_addr, &len);

            if (bytes <= 0) {
                Logger::info("Client ", inet_ntoa(client_addr.sin_addr), ":", ntohs(client_addr.sin_port), " disconnected");
                close_socket(client_socket);
                searching_clients.erase(std::remove(searching_clients.begin(), searching_clients.end(), client_socket), searching_clients.end());
                it = clients.erase(it);
                continue;
            }

            if (bytes < MULTI_PONG_SERVER_BUFFER) {
                buffer[bytes] = '\0';
            }

            Logger::debug("Received: ", buffer);

            Message message;
            if (!message.ParseFromArray(buffer, bytes)) {
				Logger::warning("Failed to process data into a protobuf message: ", buffer);
                it++;
                continue;
            }

            switch (message.content_case()) {
                case Message::kSearch:
                    Logger::info("Added client ", inet_ntoa(client_addr.sin_addr), ":", ntohs(client_addr.sin_port), " as a searching player");
                    searching_clients.push_back(client_socket);
                    break;
                default:
                    Logger::warning("Invalid message type ", message.content_case(), " from client ", inet_ntoa(client_addr.sin_addr), ":", ntohs(client_addr.sin_port));
                    break;
			};

            it++;
        }
    }
}

void Coordinator::send_message_to_client(socket_t client_socket, const Message& message) {
    send(client_socket, message.SerializeAsString().c_str(), message.ByteSizeLong(), 0);
}

// todo: move to a single main.cpp which takes --server, --coordinator and --client as command line args with conditional cmake builds
// int main() {
//     Coordinator coordinator;
//     return 0;
// }