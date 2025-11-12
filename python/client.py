import logging
from tkinter import Tk, Frame
from socket import socket, AF_INET, SOCK_DGRAM, SOCK_STREAM
from threading import Thread
from protobufs.pong_pb2 import Search, Match, Join, Message, Direction, Movement


MULTI_PONG_SERVER_BUFFER = 512
MULTI_PONG_COORDINATOR_ADDRESS = ("127.0.0.1", 4999)
MULTI_PONG_BALL_WIDTH = 0.015
MULTI_PONG_BALL_HEIGHT = 0.015 * (16.0 / 9.0)
MULTI_PONG_PADDLE_WIDTH = 0.015
MULTI_PONG_PADDLE_HEIGHT = 0.15
MULTI_PONG_PADDLE_HORIZONTAL_PADDING = 0.1


class Client:
    def __init__(self):
        self.logger = logging.getLogger(__name__)
        self.window = Tk()
        self.window.geometry("1280x720")
        self.window.configure(bg='#282828')
        self.window.title('Pong - searching for a match... - press F to toggle fullscreen')

        self.coordinator = socket(AF_INET, SOCK_STREAM)
        self.server = socket(AF_INET, SOCK_DGRAM)
        self.server.bind(("0.0.0.0", 0))
        self.address = None
        self.state = None
        self.token = None
        self.last_frame = 0
        self.last_processed_frame = 0
        self.identifier = 0

        self.connect()
        Thread(target=self.listen_coordinator).start()

        self.send_message_to_coordinator(Search())

        self.paddles = [Frame(self.window, bg="#FFFFFF"), Frame(self.window, bg="#FFFFFF")]
        self.ball = Frame(self.window, bg="#FFFFFF")

        self.place_centred(self.ball, relx=0.5, rely=0.5, relwidth=MULTI_PONG_BALL_WIDTH, relheight=MULTI_PONG_BALL_HEIGHT)
        self.place_centred(self.paddles[0], relx=MULTI_PONG_PADDLE_HORIZONTAL_PADDING, rely=0.5, relwidth=MULTI_PONG_PADDLE_WIDTH, relheight=MULTI_PONG_PADDLE_HEIGHT)
        self.place_centred(self.paddles[1], relx=1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING, rely=0.5, relwidth=MULTI_PONG_PADDLE_WIDTH, relheight=MULTI_PONG_PADDLE_HEIGHT)

        self.window.bind("<Up>", lambda _: self.send_move(Direction.UP))
        self.window.bind("<Down>", lambda _: self.send_move(Direction.DOWN))
        self.window.bind("<KeyRelease-Up>", lambda _: self.send_move(Direction.STOP))
        self.window.bind("<KeyRelease-Down>", lambda _: self.send_move(Direction.STOP))
        self.window.bind("<F>", self.toggle_fullscreen)
        self.window.bind("<f>", self.toggle_fullscreen)

        self.update_loop()

        self.window.mainloop()

    def connect(self):
        try:
            self.coordinator.connect(MULTI_PONG_COORDINATOR_ADDRESS)
            self.logger.info("Connected to game coordinator at %s:%s", *MULTI_PONG_COORDINATOR_ADDRESS)
        except ConnectionRefusedError:
            self.logger.error("Failed to connect to game coordinator at %s:%s", *MULTI_PONG_COORDINATOR_ADDRESS)
            exit(-1)

    def listen_coordinator(self):
        while True:
            try:
                received_data = self.coordinator.recv(MULTI_PONG_SERVER_BUFFER)
                message = Message()
                message.ParseFromString(received_data)
                content = message.WhichOneof("content")

                if content == "match":
                    self.handle_match(message.match)
            except:
                self.logger.exception("Unhandled exception in coordinator listen loop")

    def send_message_to_coordinator(self, data):
        message = Message()
        getattr(message, type(data).__name__.lower()).CopyFrom(data)
        self.coordinator.send(message.SerializeToString())

    def send_message_to_server(self, data):
        if self.address:
            message = Message()
            getattr(message, type(data).__name__.lower()).CopyFrom(data)
            self.server.sendto(message.SerializeToString(), self.address)

    def handle_match(self, match: Match):
        self.address = (match.host, match.port)
        self.identifier = match.player.identifier
        self.token = match.token

        Thread(target=self.listen_server, daemon=True).start()

        join = Join(token=self.token)
        self.send_message_to_server(join)

    def listen_server(self):
        while True:
            try:
                received_data = self.server.recv(MULTI_PONG_SERVER_BUFFER)
                message = Message()
                message.ParseFromString(received_data)
                content = message.WhichOneof("content")

                match content:
                    case "state":
                        if message.state.token != self.token:
                            self.logger.warning("Received state with invalid token %s (expected: %s)", message.state.token, self.token)
                            continue
                        if message.state.frame >= self.last_frame:
                            self.last_frame = message.state.frame
                            self.state = message.state
                        else:
                            self.logger.info("Discarding frame %s because it's out of sync (last frame: %s)", message.state.frame, self.last_frame)
                    case _:
                        pass
            except:
                self.logger.exception("Unhandled exception in server listen loop")

    def update_loop(self):
        if self.last_frame > self.last_processed_frame:
            self.last_processed_frame = self.last_frame

            self.place_centred(self.ball, self.state.ball.x, self.state.ball.y, MULTI_PONG_BALL_WIDTH, MULTI_PONG_BALL_HEIGHT)
            self.place_centred(self.paddles[0], MULTI_PONG_PADDLE_HORIZONTAL_PADDING, self.state.player_1.paddle_location, MULTI_PONG_PADDLE_WIDTH, MULTI_PONG_PADDLE_HEIGHT)
            self.place_centred(self.paddles[1], 1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING, self.state.player_2.paddle_location, MULTI_PONG_PADDLE_WIDTH, MULTI_PONG_PADDLE_HEIGHT)

            self.window.title(f'Pong - [{self.state.player_1.score} - {self.state.player_2.score}] - Press F to toggle fullscreen')

        self.window.after(8, self.update_loop)

    def toggle_fullscreen(self, _):
        self.window.attributes("-fullscreen", not self.window.wm_attributes("-fullscreen"))

    def send_move(self, move):
        if not self.token:
            return

        movement = Movement(token=self.token, direction=move)
        self.send_message_to_server(movement)

    def place_centred(self, widget, relx, rely, relwidth, relheight):
        widget.place(relx=relx - relwidth / 2, rely=rely - relheight / 2, relwidth=relwidth, relheight=relheight)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='[%(asctime)s] - %(levelname)s: %(message)s')
    Client()
