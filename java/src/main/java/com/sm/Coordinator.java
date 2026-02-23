package com.sm;

import com.google.protobuf.InvalidProtocolBufferException;
import com.sm.protobufs.Pong.*;

import java.io.IOException;
import java.net.*;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import static com.sm.Constants.*;


public class Coordinator {

    private static final Logger logger = LoggerFactory.getLogger(Coordinator.class);

    record ServerAddress(String host, int port) {}

    ServerSocket socket;
    DatagramSocket serverSocket;
    String secret = "";
    ArrayList<Socket> clients = new ArrayList<>();
    ArrayList<Socket> searchingClients = new ArrayList<>();
    HashMap<ServerAddress, Status.Phase> serverList = new HashMap<>();
    HashMap<String, InetAddress> cachedHosts = new HashMap<>();

    public Coordinator() {
        serverList.put(new ServerAddress("127.0.0.1", 5000), Status.Phase.UNKNOWN);
        serverList.put(new ServerAddress("127.0.0.1", 5001), Status.Phase.UNKNOWN);
        serverList.put(new ServerAddress("127.0.0.1", 5002), Status.Phase.UNKNOWN);
        serverList.put(new ServerAddress("127.0.0.1", 5003), Status.Phase.UNKNOWN);
        serverList.put(new ServerAddress("127.0.0.1", 5004), Status.Phase.UNKNOWN);

        try {
            socket = new ServerSocket(MULTI_PONG_COORDINATOR_PORT);
            serverSocket = new DatagramSocket();
            serverSocket.setSoTimeout(1000);
        } catch (IOException e) {
            logger.error("Error during socket creation: ", e);
            return;
        }

        new Thread(this::checkStatus).start();  // maybe use something else like tasks/ExecutorService

        while (true);
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
