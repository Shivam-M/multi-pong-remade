package com.sm;

import com.sm.render.SwingRenderer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.net.InetSocketAddress;

public class Main {

    private static final Logger logger = LoggerFactory.getLogger(Main.class);

    public static void main(String[] args) {
        // Coordinator coordinator = new Coordinator();
        // Server server = new Server();
        Client client = new Client(new InetSocketAddress("127.0.0.1", 4999), SwingRenderer.class);
    }

}