package com.sm;

import com.google.protobuf.InvalidProtocolBufferException;
import com.sm.protobufs.Pong.*;

import java.io.IOException;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.*;
import java.util.concurrent.TimeUnit;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import static com.sm.Constants.*;


public class Coordinator {

    private static final Logger logger = LoggerFactory.getLogger(Coordinator.class);

    record ServerAddress(String host, int port) {}

    Selector coordinatorSelector;
    DatagramSocket serverSocket;
    String secret = "";
    Deque<SocketChannel> searchingClients = new ArrayDeque<>();
    HashMap<ServerAddress, Status.Phase> serverList = new HashMap<>();
    HashMap<String, InetAddress> cachedHosts = new HashMap<>();

    public Coordinator() {
        serverList.put(new ServerAddress("127.0.0.1", 5000), Status.Phase.UNKNOWN);
        serverList.put(new ServerAddress("127.0.0.1", 5001), Status.Phase.UNKNOWN);
        serverList.put(new ServerAddress("127.0.0.1", 5002), Status.Phase.UNKNOWN);
        serverList.put(new ServerAddress("127.0.0.1", 5003), Status.Phase.UNKNOWN);
        serverList.put(new ServerAddress("127.0.0.1", 5004), Status.Phase.UNKNOWN);

        try {
            coordinatorSelector = Selector.open();
            ServerSocketChannel serverChannel = ServerSocketChannel.open();
            serverChannel.bind(new InetSocketAddress(MULTI_PONG_COORDINATOR_PORT));
            serverChannel.configureBlocking(false);
            serverChannel.register(coordinatorSelector, SelectionKey.OP_ACCEPT);

            logger.debug("Bound coordinator server channel to port {}", MULTI_PONG_COORDINATOR_PORT);

            serverSocket = new DatagramSocket();
            serverSocket.setSoTimeout(MULTI_PONG_SERVER_CHECK_TIMEOUT * 1000);
        } catch (IOException e) {
            logger.error("Error during socket creation: ", e);
            return;
        }

        new Thread(this::checkStatus).start();  // maybe use something else like tasks/ExecutorService
        new Thread(this::listen).start();

        while (true);
    }

    void listen() {
        logger.info("Started listening on 0.0.0.0:{}...", MULTI_PONG_COORDINATOR_PORT);

        while (!Thread.currentThread().isInterrupted()) {
            try {
                coordinatorSelector.select();
                Iterator<SelectionKey> keys = coordinatorSelector.selectedKeys().iterator();

                while (keys.hasNext()) {
                    SelectionKey key = keys.next();
                    keys.remove();

                    try {
                        if (key.isAcceptable()) {
                            ServerSocketChannel server = (ServerSocketChannel) key.channel();
                            SocketChannel client = server.accept();
                            InetSocketAddress address = (InetSocketAddress) client.getRemoteAddress();

                            client.configureBlocking(false);
                            client.register(coordinatorSelector, SelectionKey.OP_READ);

                            logger.info("Client {}:{} connected", address.getAddress().getHostAddress(), address.getPort());
                        } else if (key.isReadable()) {
                            SocketChannel client = (SocketChannel) key.channel();
                            ByteBuffer buffer = ByteBuffer.allocate(MULTI_PONG_SERVER_BUFFER);
                            InetSocketAddress address = (InetSocketAddress) client.getRemoteAddress();
                            int bytesRead = client.read(buffer);

                            if (bytesRead == -1) {
                                handleDisconnection(key);
                                continue;
                            }

                            if (bytesRead == 0) continue;

                            // need to deal with multiple packets across all languages: Message.parseDelimitedFrom()?
                            Message message = Message.parseFrom(buffer.flip());
                            logger.info("{}", message);

                            if (message.hasSearch()) {
                                if (!searchingClients.contains(client)) {
                                    searchingClients.add(client);
                                    logger.info("Add client {}:{} as a searching player", address.getAddress().getHostAddress(), address.getPort());
                                }
                            } else {
                                logger.warn("Invalid message type received from client {}:{}", address.getAddress().getHostAddress(), address.getPort());
                            }
                        }
                    } catch (IOException e) {
                        handleDisconnection(key);
                    }
                }
            } catch (Exception e) {
                logger.error("Exception whilst listening for clients: ", e);
            }
        }
    }

    void handleDisconnection(SelectionKey key) {
        try {
            SocketChannel client = (SocketChannel) key.channel();
            InetSocketAddress address = (InetSocketAddress) client.getRemoteAddress();

            key.cancel();
            client.close();

            logger.info("Client {}:{} disconnected", address.getAddress().getHostAddress(), address.getPort());
        } catch (IOException e) {
            logger.error("Error while handling a client disconnection: ", e);
        }
    }

    void checkStatus() {
        Message message = Message.newBuilder().setQuery(Query.getDefaultInstance()).build();
        DatagramPacket queryPacket = new DatagramPacket(message.toByteArray(), message.toByteArray().length);

        try (DatagramSocket querySocket = new DatagramSocket()) {
            querySocket.setSoTimeout(MULTI_PONG_SERVER_CHECK_TIMEOUT * 1000);

            while (!Thread.currentThread().isInterrupted()) {
                for (ServerAddress serverAddress : serverList.keySet()) {
                    Message receivedMessage = sendPacketToServer(serverAddress, queryPacket, querySocket);

                    if (receivedMessage == null || !receivedMessage.hasStatus()) {
                        logger.info("Server {}:{} is unresponsive", serverAddress.host, serverAddress.port);
                        continue;
                    }

                    Status.Phase status = receivedMessage.getStatus().getPhase();
                    serverList.put(serverAddress, status);

                    if (status.equals(Status.Phase.WAITING)) {
                        logger.info("Server {}:{} is available", serverAddress.host, serverAddress.port);
                    } else {
                        logger.info("Server {}:{} is busy", serverAddress.host, serverAddress.port);
                    }
                }

                matchmake();

                try {
                    TimeUnit.SECONDS.sleep(MULTI_PONG_SERVER_CHECK_INTERVAL);
                } catch (InterruptedException ie) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }
        } catch (SocketException e) {
            logger.error("Error creating a socket for query requests", e);
        }
    }

    void matchmake() {
        if (searchingClients.size() < 2) return;

        Map.Entry<ServerAddress, Tokens> preparedServer = getPreparedServer();

        if (preparedServer == null) return;

        ServerAddress serverAddress = preparedServer.getKey();
        Tokens serverTokens = preparedServer.getValue();

        Player.Identifier[] players = {Player.Identifier.PLAYER_1, Player.Identifier.PLAYER_2};
        String[] tokens = {serverTokens.getToken1(), serverTokens.getToken2()};

        for (int i = 0; i < 2; i++) {
            Player player = Player.newBuilder()
                    .setPaddleDirection(Direction.STOP)
                    .setPaddleLocation(0.0f)
                    .setScore(0)
                    .setIdentifier(players[i]).build();

            Match match = Match.newBuilder()
                    .setHost(serverAddress.host())
                    .setPort(serverAddress.port())
                    .setToken(tokens[i])
                    .setPlayer(player)
                    .build();

            sendMessageToClient(searchingClients.pop(), Message.newBuilder().setMatch(match).build());
        }
    }

    Map.Entry<ServerAddress, Tokens> getPreparedServer() {
        Message prepareMessage = Message.newBuilder().setPrepare(Prepare.newBuilder().setSecret(secret)).build();

        for (Map.Entry<ServerAddress, Status.Phase> server: serverList.entrySet()) {
            if (server.getValue().equals(Status.Phase.WAITING)) {
                Message tokensMessage = sendMessageToServer(server.getKey(), prepareMessage, serverSocket);

                if (tokensMessage == null || !tokensMessage.hasTokens()) continue;

                return Map.entry(server.getKey(), tokensMessage.getTokens());
            }
        }
        return null;
    }

    void sendMessageToClient(SocketChannel socketChannel, Message message) {
        try {
            ByteBuffer buffer = ByteBuffer.wrap(message.toByteArray());
            socketChannel.write(buffer);
        } catch (IOException e) {
            logger.error("Error sending message {} to client {}", message, socketChannel, e);
        }
    }

    Message sendMessageToServer(ServerAddress serverAddress, Message message, DatagramSocket socket) {
        DatagramPacket messagePacket = new DatagramPacket(message.toByteArray(), message.toByteArray().length);
        return sendPacketToServer(serverAddress, messagePacket, socket);
    }

    Message sendPacketToServer(ServerAddress serverAddress, DatagramPacket packet, DatagramSocket socket) {
        DatagramPacket responsePacket = new DatagramPacket(new byte[MULTI_PONG_SERVER_BUFFER], MULTI_PONG_SERVER_BUFFER);

        try {
            packet.setAddress(resolveHostname(serverAddress.host));
            packet.setPort(serverAddress.port);
            socket.send(packet);
            socket.receive(responsePacket);
            return Message.parseFrom(ByteBuffer.wrap(responsePacket.getData(),0, responsePacket.getLength()));
        } catch (InvalidProtocolBufferException e) {
            logger.warn("Received malformed message from {}:{}", serverAddress.host, serverAddress.port, e);
        } catch (UnknownHostException e) {
            logger.error("Unable to resolve hostname {}", serverAddress.host, e);
        } catch (IOException e) {
            logger.trace("IO error while communicating with {}:{}", serverAddress.host, serverAddress.port, e);
        }

        return null;
    }

    InetAddress resolveHostname(String host) throws UnknownHostException {
        InetAddress address = cachedHosts.get(host);

        if (address == null) {
            address = InetAddress.getByName(host);
            cachedHosts.put(host, address);
        }

        return address;
    }

}
