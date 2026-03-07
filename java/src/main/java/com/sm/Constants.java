package com.sm;

import java.net.InetSocketAddress;

public class Constants {

    public static final InetSocketAddress MULTI_PONG_COORDINATOR_ADDRESS = new InetSocketAddress("127.0.0.1", 4999);
    public static final int MULTI_PONG_COORDINATOR_PORT = 4999;
    public static final int MULTI_PONG_SERVER_PORT = 5001;
    public static final int MULTI_PONG_SERVER_BUFFER = 512;
    public static final int MULTI_PONG_SERVER_CHECK_INTERVAL = 5;
    public static final int MULTI_PONG_SERVER_CHECK_TIMEOUT = 1;
    public static final int MULTI_PONG_SERVER_UPDATE_RATE = 1000 / 128;

    public static final float MULTI_PONG_PADDLE_SPEED = 0.015f;
    public static final float MULTI_PONG_PADDLE_HORIZONTAL_PADDING = 0.1f;
    public static final float MULTI_PONG_PADDLE_HIT_EDGE_FACTOR = 0.5f;

    public static final float MULTI_PONG_BALL_VELOCITY = 0.0025f;
    public static final float MULTI_PONG_BALL_WIDTH = 0.015f;
    public static final float MULTI_PONG_BALL_HEIGHT = 0.015f * (16.0f / 9.0f);
    public static final float MULTI_PONG_PADDLE_WIDTH = 0.015f;
    public static final float MULTI_PONG_PADDLE_HEIGHT = 0.15f;

}
