import logging
from select import select
from time import sleep
from os import getenv
from socket import socket, AF_INET, SOCK_DGRAM, SOCK_STREAM
from threading import Thread
from protobufs.pong_pb2 import Player, Query, Status, Match, Prepare, Message


MULTI_PONG_COORDINATOR_PORT = 4999
MULTI_PONG_SERVER_BUFFER = 512


class Coordinator:
    def __init__(self):
        self.logger = logging.getLogger(__name__)
        self.socket = socket(AF_INET, SOCK_STREAM)
        self.socket.bind(("0.0.0.0", MULTI_PONG_COORDINATOR_PORT))
        self.socket.listen(10)
        self.secret = getenv("MULTI_PONG_SECRET", "1")
        self.clients = [self.socket]
        self.searching_clients = []
        self.server_list = {
            ("127.0.0.1", 5000): -1,
            ("127.0.0.1", 5001): -1,
            ("127.0.0.1", 5002): -1,
            ("127.0.0.1", 5003): -1,
            ("127.0.0.1", 5004): -1,
        }

        Thread(target=self.check_status).start()
        self.listen()

    def _check_status(self):
        query_message = Message(query=Query())
        for server in self.server_list:
            self.send_message_to_server(server, query_message)

    def check_status(self):
        while True:
            self._check_status()
            self.matchmake()
            sleep(5)

    def send_message_to_server(self, server, message: Message):
        try:
            server_socket = socket(AF_INET, SOCK_DGRAM)
            server_socket.connect(server)
            server_socket.settimeout(1)
            server_socket.send(message.SerializeToString())

            received_data, _ = server_socket.recvfrom(MULTI_PONG_SERVER_BUFFER)

            message = Message()
            message.ParseFromString(received_data)
            content = message.WhichOneof("content")

            match content:
                case "status":
                    if message.status.phase == Status.Phase.WAITING:
                        self.logger.info("Server %s:%s is available", *server)
                    else:
                        self.logger.info("Server %s:%s is busy", *server)
                    self.server_list[server] = message.status.phase
                    return message.status
                case "tokens":
                    self.server_list[server] = Status.Phase.STARTED
                    return message.tokens
                case _:
                    self.logger.warning("Invalid message type %s from %s:%s", content, *server)

        except Exception:
            self.server_list[server] = -1
            self.logger.info("Server %s:%s is unresponsive", *server)

        finally:
            server_socket.close()

    def get_prepared_server(self):
        prepare_message = Message(prepare=Prepare(secret=self.secret))
        for server, status in self.server_list.items():
            if status != Status.Phase.WAITING:
                continue
            if tokens := self.send_message_to_server(server, prepare_message):
                return (server, tokens)
        return None

    def listen(self):
        self.logger.info("Started listening on %s:%s", "0.0.0.0", MULTI_PONG_COORDINATOR_PORT)

        while True:
            try:
                read_sockets, _, _ = select(self.clients, [], [])
                for socket in read_sockets:
                    if socket == self.socket:
                        sockfd, address = self.socket.accept()
                        self.clients.append(sockfd)
                        self.logger.info("Client %s:%s connected", *address)
                    else:
                        try:
                            if not (received_data := socket.recv(MULTI_PONG_SERVER_BUFFER)):
                                continue

                            message = Message()
                            message.ParseFromString(received_data)
                            content = message.WhichOneof("content")
                            
                            match content:
                                case "search":
                                    self.searching_clients.append(socket)
                                    self.logger.info("Add client %s:%s as a searching player", *socket.getpeername())
                                case _:
                                    self.logger.warning("Invalid message type %s from %s:%s", content, *socket.getpeername())
                        except:
                            socket.close()
                            self.clients.remove(socket)
                            if socket in self.searching_clients:
                                self.searching_clients.remove(socket)
                            self.logger.info("Client %s:%s disconnected", *socket.getpeername())
            except OSError as e:
                self.logger.debug("Socket exception - likely listening too early: %s", e)
            except Exception:
                self.logger.exception("Unhandled exception in client listen loop")


    def matchmake(self):
        if len(self.searching_clients) < 2:
            return

        if not (prepared_server := self.get_prepared_server()):
            return

        server, tokens = prepared_server

        for token, player_id in [(tokens.token_1, Player.Identifier.PLAYER_1), (tokens.token_2, Player.Identifier.PLAYER_2)]:
            player = Player(identifier=player_id, paddle_direction=0, paddle_location=0.5, score=0)
            match = Match(host=server[0], port=server[1], token=token, player=player)
            client = self.searching_clients.pop(0)
            self.send_message_to_client(client, match)
            self.logger.info("Forwarded match on %s:%s to client with token %s", *server, token)

    def send_message_to_client(self, client, data):
        message = Message()
        getattr(message, type(data).__name__.lower()).CopyFrom(data)
        client.send(message.SerializeToString())


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='[%(asctime)s] - %(levelname)s: %(message)s')
    Coordinator()
