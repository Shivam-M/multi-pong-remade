#include "server.h"
#include "tools/logger.h"

#include <thread>
#include <cstdlib>

using namespace multi_pong;

Server::Server() {
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

Server::~Server() {
    close_socket(server_socket);
#ifdef _WIN32
    WSACleanup();
#endif
}

Tokens Server::generate_tokens() {
    Tokens tokens;
    tokens.set_token_1(std::to_string(std::rand()));
    tokens.set_token_2(std::to_string(std::rand()));

    while (tokens.token_2() == tokens.token_1()) {
        tokens.set_token_2(std::to_string(std::rand()));
    }

    Logger::info("Generated tokens ", tokens.token_1(), " / ", tokens.token_2());
    return tokens;
}

void Server::listen() {
    Logger::info("Started listening on 0.0.0.0:", MULTI_PONG_SERVER_PORT);

    Message message;
    sockaddr_in address{};
    socklen_t address_length = sizeof(address);
    char buffer[MULTI_PONG_SERVER_BUFFER];

    while (true) {
        int received_data = recvfrom(server_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&address, &address_length);

        if (received_data < 0) {
            continue;
        }

        if (!message.ParseFromArray(buffer, static_cast<int>(received_data))) continue;
        
        Logger::debug("Message parsed successfully, type: ", message.content_case());
        
        switch (message.content_case()) {
            case Message::kPrepare:
                handle_prepare(message.prepare(), address);
                break;
            case Message::kJoin:
                handle_join(message.join(), address);
                break;
            case Message::kMovement:
                handle_movement(message.movement(), address);
                break;
            case Message::kQuery:
                handle_query(address);
                break;
            default:
                break;
        }
    }
}

void Server::handle_query(const sockaddr_in& address) {
    send(status, address);
}

void Server::handle_prepare(const Prepare& prepare, const sockaddr_in& address) {
    Logger::info("Received preparation request from ", inet_ntoa(address.sin_addr), ":", ntohs(address.sin_port));

    if (status.phase() != Status::WAITING) {
        return;
    }

    if (secret == "") {
        Logger::warning("No secret set - skipping authentication with ", inet_ntoa(address.sin_addr), ":", ntohs(address.sin_port));
    } else if (secret != prepare.secret()) {
        return;
    }

    Logger::info("Forwarding tokens after transitioning into a prepared state");
    status.set_phase(Status::PREPARING);
    send(tokens, address);
}

void Server::handle_join(const Join& join, const sockaddr_in& address) {
    Logger::info("Received join request from client ", inet_ntoa(address.sin_addr), ":", ntohs(address.sin_port));

    if (status.phase() != Status::PREPARING) {
        return;
    }

    auto player_id = get_player_id_by_token(join.token());
    if (!player_id) {
        return;
    }

    std::string token = join.token();

    Player& player = clients[token];
    player.set_identifier(*player_id);
    player.set_paddle_direction(Direction::STOP);
    player.set_paddle_location(0.5f);
    player.set_score(0);

    token_addresses[token] = address;

    Logger::info("Registered client ", inet_ntoa(address.sin_addr), ":", ntohs(address.sin_port), " as player ", static_cast<int>(*player_id));

    if (clients.size() >= 2) {
        start_match();
    }
}

void Server::handle_movement(const Movement& movement, const sockaddr_in& address) {
    if (status.phase() != Status::STARTED) {
        return;
    }

    auto player_id = get_player_id_by_token(movement.token());
    if (!player_id) {
        return;
    }

    clients[movement.token()].set_paddle_direction(movement.direction());
    Logger::info("Player ", static_cast<int>(*player_id), " sent movement direction ", movement.direction());
}

std::string Server::get_token_by_player_id(const Player::Identifier& player_id) {
    for (const auto& [token, player] : clients) {
        if (player.identifier() == player_id) {
            return token;
        }
    }
    return "";
}

std::optional<Player::Identifier> Server::get_player_id_by_token(const std::string& token) {
    if (token != tokens.token_1() && token != tokens.token_2())
        return std::nullopt;

    return (token == tokens.token_1()) ? Player::PLAYER_1 : Player::PLAYER_2;
}

void Server::start_match() {
    Logger::info("All players have joined - starting match");
    status.set_phase(Status::STARTED);
    std::thread game_thread(&Server::game_loop, this);
    game_thread.detach();
}

void Server::game_loop() {
    const float paddle_speed = 0.02f;
    const float ball_speed_increment = 0.0005f;
    Ball* ball = state.mutable_ball();
    
    reset_ball();

    while (status.phase() == Status::STARTED) {

        ball->set_x(ball->x() + ball_velocity[0]);
        ball->set_y(ball->y() + ball_velocity[1]);

        if (ball->y() <= 0.0f || ball->y() >= 1.0f) {
            ball_velocity[1] *= -1;
        }

        if (ball->x() <= 0.0f || ball->x() >= 1.0f) {
            Player::Identifier player_id = ball->x() < 0.0f ? Player::PLAYER_2 : Player::PLAYER_1;
            std::string scoring_token = get_token_by_player_id(player_id);
            clients[scoring_token].set_score(clients[scoring_token].score() + 1);
            reset_ball();
        }

        for (auto& [token, player] : clients) {
            float paddle_location = player.paddle_location();

            if (player.paddle_direction() == Direction::UP) {
                paddle_location -= paddle_speed;
            } else if (player.paddle_direction() == Direction::DOWN) {
                paddle_location += paddle_speed;
            }
            
            player.set_paddle_location(std::clamp(paddle_location, 0.0f, 1.0f));
        }

        float relative_hit = 0.0f;
        bool is_ball_on_left_side = ball->x() <= 0.5f;
        float paddle_x = is_ball_on_left_side ? MULTI_PONG_PADDLE_HORIZONTAL_PADDING : 1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING;
        float paddle_y = clients[is_ball_on_left_side ? tokens.token_1() : tokens.token_2()].paddle_location();

        if (did_ball_hit_paddle(paddle_x, paddle_y, relative_hit)) {
            ball_velocity[0] *= -1 - (MULTI_PONG_PADDLE_HIT_EDGE_FACTOR * abs(relative_hit - 0.5));
        }

        state.set_frame(state.frame() + 1);
        send_state_to_all_players();

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 128));
    }
}

void Server::reset_ball() {
    constexpr float INITIAL_BALL_VELOCITY = 0.0025f;
    ball_velocity[0] = (std::rand() % 2 ? -1 : 1) * INITIAL_BALL_VELOCITY;
    ball_velocity[1] = (std::rand() % 2 ? -1 : 1) * INITIAL_BALL_VELOCITY;
    state.mutable_ball()->set_x(0.5f);
    state.mutable_ball()->set_y(0.5f);
}

bool Server::did_ball_hit_paddle(float paddle_x, float paddle_y, float& relative_hit) {
    auto* ball = state.mutable_ball();
    float ball_x = ball->x();
    float ball_y = ball->y();

    float ball_left = ball_x - MULTI_PONG_BALL_WIDTH * 0.5f;
    float ball_right = ball_x + MULTI_PONG_BALL_WIDTH * 0.5f;
    float ball_top = ball_y - MULTI_PONG_BALL_HEIGHT * 0.5f;
    float ball_bottom = ball_y + MULTI_PONG_BALL_HEIGHT * 0.5f;

    float paddle_left = paddle_x - MULTI_PONG_PADDLE_WIDTH * 0.5f;
    float paddle_right = paddle_x + MULTI_PONG_PADDLE_WIDTH * 0.5f;
    float paddle_top = paddle_y - MULTI_PONG_PADDLE_HEIGHT * 0.5f;
    float paddle_bottom = paddle_y + MULTI_PONG_PADDLE_HEIGHT * 0.5f;

    if (ball_left < paddle_right && ball_right > paddle_left && ball_top < paddle_bottom && ball_bottom > paddle_top) {
        float hit_y = (std::max(ball_top, paddle_top) + std::min(ball_bottom, paddle_bottom)) * 0.5f;
        relative_hit = (hit_y - paddle_top) / MULTI_PONG_PADDLE_HEIGHT;
        return true;
    }

    return false;
}

void Server::send_state_to_all_players() {
    auto it = clients.begin();
    state.mutable_player_1()->CopyFrom(it->second);
    it++;
    state.mutable_player_2()->CopyFrom(it->second);
    for (const auto& [token, player] : clients) {
        state.set_token(token);
        send(state, token_addresses[token]);
    }
}

template<typename T>
void Server::send(const T& data, const sockaddr_in& address) {
    Message message;

    if constexpr (std::is_same_v<T, Status>) {
        message.mutable_status()->CopyFrom(data);
    } else if constexpr (std::is_same_v<T, State>) {
        message.mutable_state()->CopyFrom(data);
    } else if constexpr (std::is_same_v<T, Tokens>) {
        message.mutable_tokens()->CopyFrom(data);
    } else {
        return;
    }

    std::string serialised_message;
    message.SerializeToString(&serialised_message);
    sendto(server_socket, serialised_message.data(), serialised_message.size(), 0, (struct sockaddr*)&address, sizeof(address));
}

// todo: move to a single main.cpp which takes --server, --coordinator and --client as command line args with conditional cmake builds
// int main() {
//     Server server;
//     return 0;
// }
