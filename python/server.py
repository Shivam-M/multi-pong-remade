import logging
from time import sleep
from random import choice
from os import getenv
from secrets import token_hex
from socket import socket, AF_INET, SOCK_DGRAM
from threading import Thread
from protobufs.pong_pb2 import Player, Ball, Tokens, Status, Join, Prepare, Movement, State, Message, Direction


MULTI_PONG_SERVER_PORT = 5001
MULTI_PONG_SERVER_BUFFER = 512
MULTI_PONG_SERVER_UPDATE_RATE = 1 / 128
MULTI_PONG_BALL_WIDTH = 0.015
MULTI_PONG_BALL_HEIGHT = 0.015 * (16.0 / 9.0)
MULTI_PONG_PADDLE_WIDTH = 0.015
MULTI_PONG_PADDLE_HEIGHT = 0.15
MULTI_PONG_PADDLE_SPEED = 0.015
MULTI_PONG_PADDLE_HORIZONTAL_PADDING = 0.1
MULTI_PONG_PADDLE_HIT_EDGE_FACTOR = 0.5


class Server:
    def __init__(self):
        self.logger = logging.getLogger(__name__)
        self.tokens = self.generate_tokens()
        self.secret = getenv("MULTI_PONG_SECRET", None)
        self.ball_velocity = [0.0, 0.0]
        self.state = State(ball=Ball(x=0.5, y=0.5), frame=0)
        self.status = Status(phase=Status.Phase.WAITING)
        self.clients = {}
        self.token_addresses = {}
        self.socket = socket(AF_INET, SOCK_DGRAM)
        self.socket.bind(("0.0.0.0", MULTI_PONG_SERVER_PORT))
        self.listen()

    def generate_tokens(self):
        tokens = Tokens(token_1=token_hex(16), token_2=token_hex(16))

        while tokens.token_1 == tokens.token_2:
            tokens.token_2 = token_hex(16)

        self.logger.info("Generated tokens %s / %s", tokens.token_1, tokens.token_2)
        return tokens

    def listen(self):
        self.logger.info("Started listening on %s:%s", "0.0.0.0", MULTI_PONG_SERVER_PORT)

        while True:
            try:
                received_data, address = self.socket.recvfrom(MULTI_PONG_SERVER_BUFFER)

                message = Message()
                message.ParseFromString(received_data)
                content = message.WhichOneof("content")

                match content:
                    case "prepare":
                        self.handle_prepare(message.prepare, address)
                    case "join":
                        self.handle_join(message.join, address)
                    case "movement":
                        self.handle_movement(message.movement, address)
                    case "query":
                        self.handle_query(address)
                    case _:
                        self.logger.warning("Invalid message type %s from %s:%s", content, *address)
            except ConnectionResetError as e:
                self.logger.debug("Connection reset by peer - likely a client disconnected: %s", e)
            except Exception:
                self.logger.exception("Unhandled exception in server listen loop")
    
    def handle_query(self, address):
        self.logger.info("Received status query from %s:%s", *address)
        self.send(self.status, address)

    def handle_prepare(self, prepare: Prepare, address):
        self.logger.info("Received preparation request from %s:%s", *address)

        if self.status.phase != Status.Phase.WAITING:
            return

        if not self.secret:
            self.logger.warning("No secret set - skipping authentication with %s:%s", *address)
        elif prepare.secret != self.secret:
            return

        self.logger.info("Forwarding tokens after transitioning into a prepared state")
        self.status.phase = Status.Phase.PREPARING
        self.send(self.tokens, address)

    def handle_join(self, join: Join, address):
        self.logger.info("Received join request from client %s:%s", *address)

        if self.status.phase != Status.Phase.PREPARING:
            return

        if (player_id := self.get_player_id_from_token(join.token)) is None:
            return

        self.token_addresses[join.token] = address
        self.clients[join.token] = Player(identifier=player_id, paddle_direction=Direction.STOP, paddle_location=0.5, score=0)
        self.logger.info("Registered client %s:%s as player %d with token %s", *address, player_id, join.token)

        if len(self.clients) == 2:
            self.start_match()

    def handle_movement(self, movement: Movement, address):
        if self.status.phase != Status.Phase.STARTED:
            return

        token = movement.token

        if (player_id := self.get_player_id_from_token(token)) is None:
            return

        self.clients[token].paddle_direction = movement.direction
        self.logger.debug("Player %d sent movement direction %d", player_id, movement.direction)

    def get_player_id_from_token(self, token: str):
        if token not in [self.tokens.token_1, self.tokens.token_2]:
            return None

        return Player.Identifier.PLAYER_1 if token == self.tokens.token_1 else Player.Identifier.PLAYER_2

    def get_token_from_player_id(self, player_id):
        for token in self.clients:
            if self.clients[token].identifier == player_id:
                return token
        return None

    def start_match(self):
        self.logger.info("All players have joined - starting match")
        self.status.phase = Status.Phase.STARTED
        Thread(target=self.game_loop).start()

    def game_loop(self):
        self.reset_ball()

        while self.status.phase == Status.Phase.STARTED:
            self.state.ball.x += self.ball_velocity[0]
            self.state.ball.y += self.ball_velocity[1]

            if not (0 < self.state.ball.y < 1):
                self.ball_velocity[1] *= -1

            if not (0 < self.state.ball.x < 1):
                player_id = Player.Identifier.PLAYER_2 if self.state.ball.x < 0 else Player.Identifier.PLAYER_1
                token = self.get_token_from_player_id(player_id)
                self.clients[token].score += 1
                self.logger.info("Player %d scored - current score: [%d - %d]", player_id, self.clients[self.tokens.token_1].score, self.clients[self.tokens.token_2].score)
                self.reset_ball()

            for token in self.clients:
                player = self.clients[token]

                if player.paddle_direction == Direction.UP:
                    player.paddle_location -= MULTI_PONG_PADDLE_SPEED
                elif player.paddle_direction == Direction.DOWN:
                    player.paddle_location += MULTI_PONG_PADDLE_SPEED

                if player.paddle_location < 0:
                    player.paddle_location = 0
                elif player.paddle_location > 1:
                    player.paddle_location = 1
            
            is_ball_on_left_side = self.state.ball.x <= 0.5
            paddle_x = MULTI_PONG_PADDLE_HORIZONTAL_PADDING if is_ball_on_left_side else 1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING
            paddle_y = self.clients[self.tokens.token_1 if is_ball_on_left_side else self.tokens.token_2].paddle_location

            if (relative_hit := self.did_ball_hit_paddle(paddle_x, paddle_y)) is not None:
                self.logger.debug("Ball hit paddle at relative Y %.2f", relative_hit)
                self.ball_velocity[0] *= -1 - (MULTI_PONG_PADDLE_HIT_EDGE_FACTOR * abs(relative_hit - 0.5))

            self.state.frame += 1
            self.send_state_to_all_players()

            sleep(MULTI_PONG_SERVER_UPDATE_RATE)
    
    def reset_ball(self):
        self.ball_velocity = [choice([-0.0025, 0.0025]), choice([-0.0025, 0.0025])]
        self.state.ball.x = 0.5
        self.state.ball.y = 0.5 
    
    def did_ball_hit_paddle(self, paddle_x, paddle_y):
        ball = self.state.ball

        ball_left = ball.x - MULTI_PONG_BALL_WIDTH / 2
        ball_right = ball.x + MULTI_PONG_BALL_WIDTH / 2
        ball_top = ball.y - MULTI_PONG_BALL_HEIGHT / 2
        ball_bottom = ball.y + MULTI_PONG_BALL_HEIGHT / 2

        paddle_left = paddle_x - MULTI_PONG_PADDLE_WIDTH / 2
        paddle_right = paddle_x + MULTI_PONG_PADDLE_WIDTH / 2
        paddle_top = paddle_y - MULTI_PONG_PADDLE_HEIGHT / 2
        paddle_bottom = paddle_y + MULTI_PONG_PADDLE_HEIGHT / 2

        if (ball_left < paddle_right and ball_right > paddle_left and ball_top < paddle_bottom and ball_bottom > paddle_top):
            hit_y = (max(ball_top, paddle_top) + min(ball_bottom, paddle_bottom)) / 2
            return ((hit_y - paddle_top) / MULTI_PONG_PADDLE_HEIGHT)

        return None

    def send_state_to_all_players(self):
        self.state.player_1.CopyFrom(list(self.clients.values())[0])  # todo: refactor: store player data in state to avoid copying
        self.state.player_2.CopyFrom(list(self.clients.values())[1])
        for token in self.clients:
            self.state.token = token
            self.send(self.state, self.token_addresses[token])

    def send(self, data, address):
        self.logger.debug("Sending %s to %s:%s", str(data).strip(), *address)
        message = Message()
        getattr(message, type(data).__name__.lower()).CopyFrom(data)
        self.socket.sendto(message.SerializeToString(), address)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='[%(asctime)s] - %(levelname)s: %(message)s')
    server = Server()
