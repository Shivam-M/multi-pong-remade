package com.sm;

import com.sm.protobufs.Pong.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.awt.*;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.security.SecureRandom;
import java.util.HexFormat;
import java.util.Random;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;

import static com.sm.Constants.*;

public class Server {

    private static final Logger logger = LoggerFactory.getLogger(Server.class);
    private static final Random random = new Random();

    private DatagramSocket socket;
    private String secret = "";
    private Tokens tokens;
    private State state;
    private Status status = Status.newBuilder().setPhase(Status.Phase.WAITING).build();
    private final ConcurrentHashMap<Player.Identifier, InetSocketAddress> clients = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<Player.Identifier, Direction> playerDirections = new ConcurrentHashMap<>();
    private final float[] ballVelocity = new float[2];

    public Server(int port) {
        try {
            socket = new DatagramSocket(port);
            logger.info("Bound server socket to port {}", port);
        } catch (IOException e) {
            logger.error("Failed to create/bind socket on port {}", port, e);
        }

        generateTokens();

        playerDirections.put(Player.Identifier.PLAYER_1, Direction.STOP);
        playerDirections.put(Player.Identifier.PLAYER_2, Direction.STOP);

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

        listen();
    }

    void listen() {
        DatagramPacket receivePacket = new DatagramPacket(new byte[MULTI_PONG_SERVER_BUFFER], MULTI_PONG_SERVER_BUFFER);

        while (!Thread.currentThread().isInterrupted()) {
            try {
                socket.receive(receivePacket);
                InetSocketAddress address = new InetSocketAddress(receivePacket.getAddress(), receivePacket.getPort());
                Message message = Message.parseFrom(ByteBuffer.wrap(receivePacket.getData(),0, receivePacket.getLength()));

                switch (message.getContentCase()) {
                    case PREPARE:
                        handlePrepare(message.getPrepare(), address);
                        break;

                    case JOIN:
                        handleJoin(message.getJoin(), address);
                        break;

                    case MOVEMENT:
                        handleMovement(message.getMovement(), address);
                        break;

                    case QUERY:
                        handleQuery(address);
                        break;
                }

            } catch (com.google.protobuf.InvalidProtocolBufferException e) {
                logger.warn("Invalid message received from {}:{}", receivePacket.getAddress(), receivePacket.getPort(), e);
            } catch (IOException e) {
                logger.error("Exception", e);
            }
        }
    }

    void handleQuery(InetSocketAddress address) {
        logger.info("Received status query from {}", address);
        sendMessage(Message.newBuilder().setStatus(status).build(), address);
    }

    void handlePrepare(Prepare prepare, InetSocketAddress address) {
        logger.info("Received preparation request from {}", address);

        if (!status.getPhase().equals(Status.Phase.WAITING)) return;

        if (secret == null || secret.isBlank()) {
            logger.warn("No secret set - skipping authentication with {}", address);
        } else if (!prepare.getSecret().equals(secret)) {
            logger.warn("The secret {} from {} does not match the server's", prepare.getSecret(), address);
        }

        status = status.toBuilder().setPhase(Status.Phase.PREPARING).build();
        logger.info("Forwarding tokens after transitioning into a prepared state");
        sendMessage(Message.newBuilder().setTokens(tokens).build(), address);
    }

    void handleJoin(Join join, InetSocketAddress address) {
        logger.info("Received join request from {}", address);

        if (!status.getPhase().equals(Status.Phase.PREPARING)) return;

        Player.Identifier playerID = getPlayerIDFromToken(join.getToken());

        if (playerID == null) return;

        clients.put(playerID, address);

        logger.info("Registered client {} as player {} with token {}", address, playerID, join.getToken());

        if (clients.size() >= 2) {
            startMatch();
        }
    }

    void handleMovement(Movement movement, InetSocketAddress address) {
        Player.Identifier playerID = getPlayerIDFromToken(movement.getToken());

        if (playerID == null) return;

        playerDirections.put(playerID, movement.getDirection());
        logger.debug("{} ({}) set paddle direction to {}", playerID, address, movement.getDirection());
    }

    void startMatch() {
        logger.info("All players have joined - starting match...");
        status = status.toBuilder().setPhase(Status.Phase.STARTED).build();
        new Thread(this::gameLoop).start();
    }

    void gameLoop() {
        resetBallVelocity();

        while (status.getPhase().equals(Status.Phase.STARTED)) {
            State.Builder stateBuilder = state.toBuilder();
            stateBuilder.getPlayer1Builder().setPaddleDirection(playerDirections.get(Player.Identifier.PLAYER_1));
            stateBuilder.getPlayer2Builder().setPaddleDirection(playerDirections.get(Player.Identifier.PLAYER_2));

            Ball.Builder ball = stateBuilder.getBallBuilder()
                    .setX(state.getBall().getX() + ballVelocity[0])
                    .setY(state.getBall().getY() + ballVelocity[1]);

            updatePaddle(stateBuilder.getPlayer1Builder());
            updatePaddle(stateBuilder.getPlayer2Builder());

            if (ball.getY() <= 0.0f || ball.getY() >= 1.0f) {
                ballVelocity[1] *= -1;
            } else if (ball.getX() <= 0.0f || ball.getX() >= 1.0f) {
                if (ball.getX() >= 1.0f) {
                    stateBuilder.getPlayer1Builder().setScore(stateBuilder.getPlayer1().getScore() + 1);
                } else {
                    stateBuilder.getPlayer2Builder().setScore(stateBuilder.getPlayer2().getScore() + 1);
                }

                resetBallVelocity();
                stateBuilder.getBallBuilder().setX(0.5f).setY(0.5f);
                logger.info("Score: [{} - {}]", stateBuilder.getPlayer1().getScore(), stateBuilder.getPlayer2().getScore());
            } else {  // maybe base this off horizontal ball velocity instead
                boolean isBallOnLeftSide = ball.getX() <= 0.5f;
                float paddleX = isBallOnLeftSide ? MULTI_PONG_PADDLE_HORIZONTAL_PADDING : 1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING;
                float paddleY = isBallOnLeftSide ? stateBuilder.getPlayer1().getPaddleLocation() : stateBuilder.getPlayer2().getPaddleLocation();
                Float paddleRelativeHit = getPaddleRelativeHit(paddleX, paddleY, ball);

                if (paddleRelativeHit != null) {
                    ballVelocity[0] *= -1 - (MULTI_PONG_PADDLE_HIT_EDGE_FACTOR * Math.abs(paddleRelativeHit - 0.5f));
                }
            }

            stateBuilder.setFrame(stateBuilder.getFrame() + 1);
            state = stateBuilder.build();
            sendStateToAllPlayers();

            try {
                TimeUnit.MILLISECONDS.sleep(MULTI_PONG_SERVER_UPDATE_RATE);
            } catch (InterruptedException ie) {
                Thread.currentThread().interrupt();
                break;
            }
        }
    }

    void resetBallVelocity() {
        ballVelocity[0] = (random.nextBoolean() ? -1 : 1) * MULTI_PONG_BALL_VELOCITY;
        ballVelocity[1] = (random.nextBoolean() ? -1 : 1) * MULTI_PONG_BALL_VELOCITY;
    }

    void updatePaddle(Player.Builder player) {
        float paddleLocation = player.getPaddleLocation();

        if (player.getPaddleDirection().equals(Direction.UP)) {
            paddleLocation -= MULTI_PONG_PADDLE_SPEED;
        } else if (player.getPaddleDirection().equals(Direction.DOWN)) {
            paddleLocation += MULTI_PONG_PADDLE_SPEED;
        }

        player.setPaddleLocation(Math.clamp(paddleLocation, 0.0f, 1.0f));
    }

    Float getPaddleRelativeHit(float paddleX, float paddleY, Ball.Builder ball) {
        float ballX = ball.getX();
        float ballY = ball.getY();

        float ballLeft = ballX - MULTI_PONG_BALL_WIDTH / 2;
        float ballRight = ballX + MULTI_PONG_BALL_WIDTH / 2;
        float ballTop = ballY - MULTI_PONG_BALL_HEIGHT / 2;
        float ballBottom = ballY + MULTI_PONG_BALL_HEIGHT / 2;

        float paddleLeft = paddleX - MULTI_PONG_PADDLE_WIDTH / 2;
        float paddleRight = paddleX + MULTI_PONG_PADDLE_WIDTH / 2;
        float paddleTop = paddleY - MULTI_PONG_PADDLE_HEIGHT / 2;
        float paddleBottom = paddleY + MULTI_PONG_PADDLE_HEIGHT / 2;

        if (ballLeft < paddleRight && ballRight > paddleLeft && ballTop < paddleBottom && ballBottom > paddleTop) {
            float hitY = (Math.max(ballTop, paddleTop) + Math.min(ballBottom, paddleBottom)) / 2;
            return (hitY - paddleTop) / MULTI_PONG_PADDLE_HEIGHT;
        }

        return null;
    }

    void sendStateToAllPlayers() {
        State.Builder playerState = state.toBuilder();

        playerState.setToken(tokens.getToken1());
        sendMessage(Message.newBuilder().setState(playerState.build()).build(), clients.get(Player.Identifier.PLAYER_1));

        playerState.setToken(tokens.getToken2());
        sendMessage(Message.newBuilder().setState(playerState.build()).build(), clients.get(Player.Identifier.PLAYER_2));
    }

    void sendMessage(Message message, InetSocketAddress address) {
        byte[] messageBytes = message.toByteArray();
        DatagramPacket messagePacket = new DatagramPacket(messageBytes, messageBytes.length);
        messagePacket.setAddress(address.getAddress());
        messagePacket.setPort(address.getPort());

        try {
            socket.send(messagePacket);
        } catch (IOException e) {
            logger.error("Exception when sending message {} to client {}", messagePacket, address);
        }
    }

    Player.Identifier getPlayerIDFromToken(String token) {
        if (!(tokens.getToken1().equals(token) || tokens.getToken2().equals(token))) return null;

        return (token.equals(tokens.getToken1()) ? Player.Identifier.PLAYER_1 : Player.Identifier.PLAYER_2);
    }

    void generateTokens() {
        tokens = Tokens.newBuilder().setToken1(getRandomHexString()).setToken2(getRandomHexString()).build();

        while (tokens.getToken1().equals(tokens.getToken2())) {  // unlikely but remaining consistent
            tokens = tokens.toBuilder().setToken2(getRandomHexString()).build();
        }

        logger.info("Generated random tokens {} / {}", tokens.getToken1(), tokens.getToken2());
    }

    String getRandomHexString() {
        byte[] bytes = new byte[16];
        new SecureRandom().nextBytes(bytes);
        return HexFormat.of().formatHex(bytes);
    }

}
