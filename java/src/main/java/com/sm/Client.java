package com.sm;

import com.google.protobuf.InvalidProtocolBufferException;
import com.sm.protobufs.Pong.*;
import com.sm.render.Renderer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.*;
import java.net.*;
import java.nio.ByteBuffer;

import static com.sm.Constants.*;

public class Client {

    private static final Logger logger = LoggerFactory.getLogger(Client.class);

    private final Renderer renderer;
    private final DatagramSocket serverSocket;
    private final InetSocketAddress coordinatorAddress;
    private InetSocketAddress serverAddress;
    private Socket coordinatorSocket;
    private DataOutputStream coordinatorOutputStream;
    private DataInputStream coordinatorInputStream;

    public volatile State state;
    private String token;

    public Client(InetSocketAddress coordinator, Class<? extends Renderer> rendererClass) {
        try {
            serverSocket = new DatagramSocket();
        } catch (IOException e) {
            logger.error("Failed to create a server socket", e);
            throw new RuntimeException(e);
        }

        try {
            renderer = rendererClass.getDeclaredConstructor(Client.class).newInstance(this);
            renderer.setup();
        } catch (Exception e) {
            logger.error("Failed to initialise {} renderer", rendererClass);
            throw new RuntimeException(e);
        }

        Player.Builder player = Player.newBuilder()
                .setPaddleLocation(0.5f)
                .setPaddleDirection(Direction.STOP)
                .setScore(0);

        state = State.newBuilder()
                .setBall(Ball.newBuilder().setX(0.5f).setY(0.5f).build())
                .setPlayer1(player.setIdentifier(Player.Identifier.PLAYER_1).build())
                .setPlayer2(player.setIdentifier(Player.Identifier.PLAYER_2).build())
                .setFrame(0)
                .setToken("")
                .build();

        coordinatorAddress = coordinator;

        try {
            connectCoordinator();
        } catch (IOException e) {
            logger.error("Failed to connect to the game coordinator at {}", coordinatorAddress);
            logger.trace("Exception connecting to the game coordinator: ", e);
            return;
        }

        new Thread(renderer::renderLoop).start();

        Thread listenCoordinatorThread = new Thread(this::listenCoordinator);
        listenCoordinatorThread.start();

        sendMessageToCoordinator(Message.newBuilder().setSearch(Search.getDefaultInstance()).build());

        try {
            listenCoordinatorThread.join();
        } catch (InterruptedException e) {
            logger.trace("Exception in coordinator listener thread", e);
        }

        renderer.cleanup();
    }

    void connectCoordinator() throws IOException {
        coordinatorSocket = new Socket(coordinatorAddress.getAddress(), coordinatorAddress.getPort());
        coordinatorOutputStream = new DataOutputStream(coordinatorSocket.getOutputStream());
        coordinatorInputStream = new DataInputStream(coordinatorSocket.getInputStream());
    }

    void listenCoordinator() {
        try {
            while (!Thread.currentThread().isInterrupted()) {
                // should use parse/writeDelimitedTo for TCP, but it requires change across all languages for parity
                byte[] buffer = new byte[MULTI_PONG_SERVER_BUFFER];
                int read = coordinatorInputStream.read(buffer);

                if (read == -1) break;

                Message message = Message.parseFrom(ByteBuffer.wrap(buffer,0, read));

                if (message.hasMatch()) {
                    handleMatch(message.getMatch());
                } else {
                    logger.warn("Received an unexpected message from the coordinator: {}", message);
                }

            }
        } catch (IOException e) {
            logger.trace("Exception listening to the game coordinator", e);
        }
        logger.error("Lost connection to the game coordinator");
    }

    void sendMessageToCoordinator(Message message) {
        try {
            logger.debug("Sending {} to the coordinator...", message);
            message.writeTo(coordinatorOutputStream);
            coordinatorOutputStream.flush();
        } catch (IOException e) {
            logger.error("Failed to send message", e);
        }
    }

    void handleMatch(Match match) {
        token = match.getToken();
        serverAddress = new InetSocketAddress(match.getHost(), match.getPort());

        new Thread(this::listenServer).start();

        sendMessageToServer(Message.newBuilder().setJoin(Join.newBuilder().setToken(token).build()).build());
    }

    void listenServer() {
        DatagramPacket receivePacket = new DatagramPacket(new byte[MULTI_PONG_SERVER_BUFFER], MULTI_PONG_SERVER_BUFFER);
        InetSocketAddress packetAddress;
        while (!Thread.currentThread().isInterrupted()) {
            try {
                serverSocket.receive(receivePacket);
                packetAddress = new InetSocketAddress(receivePacket.getAddress(), receivePacket.getPort());
                Message message = Message.parseFrom(ByteBuffer.wrap(receivePacket.getData(),0, receivePacket.getLength()));

                if (message.hasState()) {
                    if (message.getState().getToken().equals(token)) {
                        state = message.getState();
                        renderer.updateState(state);
                    } else {
                        logger.warn("Received a game state message with an invalid token");
                    }
                } else {
                    logger.warn("Received an unexpected or malformed message from {}", packetAddress);
                }
            } catch (InvalidProtocolBufferException e) {
                logger.warn("Received malformed message while listening for messages from {}", serverAddress, e);
            } catch (IOException e) {
                logger.trace("IO error while listening for packets from {}", serverAddress, e);
            }
        }
    }

    void sendMessageToServer(Message message) {
        byte[] messageBytes = message.toByteArray();
        DatagramPacket messagePacket = new DatagramPacket(messageBytes, messageBytes.length);
        messagePacket.setAddress(serverAddress.getAddress());
        messagePacket.setPort(serverAddress.getPort());

        try {
            serverSocket.send(messagePacket);
        } catch (IOException e) {
            logger.error("Error sending message {} to {}", message, serverAddress);
        }
    }

    public void sendMovement(Direction direction) {
        if (serverAddress == null) return;

        Movement movement = Movement.newBuilder().setDirection(direction).setToken(token).build();
        sendMessageToServer(Message.newBuilder().setMovement(movement).build());
    }
}
