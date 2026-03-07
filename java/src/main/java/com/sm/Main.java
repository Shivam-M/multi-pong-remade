package com.sm;

import com.sm.render.JavaFXRenderer;
import com.sm.render.Renderer;
import com.sm.render.SwingRenderer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.net.InetSocketAddress;
import java.util.ArrayList;

import static com.sm.Constants.*;

public class Main {

    public static class Arguments {
        boolean invalid = false;
        boolean client = true;
        boolean server = false;
        boolean coordinator = false;
        boolean javafx = false;
        Integer port = null;
        String host = null;
        ArrayList<InetSocketAddress> serverAddresses = new ArrayList<>();
    }

    private static final Logger logger = LoggerFactory.getLogger(Main.class);

    private static InetSocketAddress parseAddress(String address) {
        String[] splitAddress = address.split(":", 2);

        if (splitAddress.length < 2) return null;

        String hostname = splitAddress[0];
        int port;

        try {
            port = Integer.parseInt(splitAddress[1]);
            if (port < 1 || port > 65535) return null;
        } catch (NumberFormatException e) {
            return null;
        }

        return new InetSocketAddress(hostname, port);
    }

    private static Arguments parseArguments(String[] args) {
        Arguments arguments = new Arguments();

        if (args.length == 0) return arguments;

        arguments.invalid = true;

        for (int i = 0; i < args.length; i++) {
            String argument = args[i];

            if (argument.equalsIgnoreCase("--client")) {
                arguments.client = true;
            } else if (argument.equalsIgnoreCase("--server")) {
                arguments.server = true;
            } else if (argument.equalsIgnoreCase("--coordinator")) {
                arguments.coordinator = true;
            } else if (argument.equalsIgnoreCase("--javafx")) {
                arguments.javafx = true;
            } else if (argument.equalsIgnoreCase("--host")) {
                if (i + 1 < args.length) {
                    arguments.host = args[++i];
                } else {
                    logger.error("Specify a valid IP address with --host <address>");
                    return arguments;
                }
            } else if (argument.equalsIgnoreCase("--port")) {
                if (i + 1 < args.length) {
                    try {
                        arguments.port = Integer.valueOf(args[++i]);
                        if (arguments.port < 1 || arguments.port > 65535) {
                            logger.error("Specify a valid port number with --port <1-65535>");
                            return arguments;
                        }
                    } catch (NumberFormatException e) {
                        logger.error("Specify a valid port number with --port <1-65535>");
                        return arguments;
                    }
                } else {
                    logger.error("Specify a valid port number with --port <1-65535>");
                    return arguments;
                }
            } else if (argument.equalsIgnoreCase("--server-address")) {
                if (i + 1 < args.length) {
                    InetSocketAddress serverAddress = parseAddress(args[++i]);
                    if (serverAddress != null) {
                        arguments.serverAddresses.add(serverAddress);
                    } else {
                        logger.error("Invalid server address: {}", args[i - 1]);
                        return arguments;
                    }
                } else {
                    logger.error("Specify multiple server addresses with --server-address <address:port>");
                    return arguments;
                }
            } else if (argument.equalsIgnoreCase("--help")) {
                System.out.println(
                        "usage: multi_pong [options]\n\n" +
                        "options:\n" +
                        "  --client                      <default>\n" +
                        "  --server                      run server\n" +
                        "  --coordinator                 run coordinator\n" +
                        "  --javafx                      [client] use the javafx renderer\n" +
                        "  --host <address>              [client] address of the coordinator\n" +
                        "  --port <1-65535>              [client] port of the coordinator\n" +
                        "                                [server/coordinator] port to listen on\n" +
                        "  --server-address <host:port>  [coordinator] (multiple) game server endpoints\n" +
                        "  --help                        show help\n");
                return arguments;
            }
        }

        arguments.invalid = false;

        return arguments;
    }

    public static void main(String[] args) {
        Arguments arguments = parseArguments(args);

        if (arguments.invalid) return;

        if (arguments.server && arguments.coordinator) {
            logger.warn("Both --server and --coordinator specified - running the server");
        }

        if (arguments.server) {
            int port = arguments.port != null ? arguments.port : MULTI_PONG_SERVER_PORT;

            new Server(port);
        }

        else if (arguments.coordinator) {
            int port = arguments.port != null ? arguments.port : MULTI_PONG_COORDINATOR_PORT;

            new Coordinator(port, arguments.serverAddresses);
        }

        else if (arguments.client) {
            InetSocketAddress coordinatorAddress = new InetSocketAddress(
                    arguments.host != null ? arguments.host : MULTI_PONG_COORDINATOR_ADDRESS.getHostString(),
                    arguments.port != null ? arguments.port : MULTI_PONG_COORDINATOR_ADDRESS.getPort()
            );

            Class<? extends Renderer> rendererClass = SwingRenderer.class;

            if (arguments.javafx) {
                rendererClass = JavaFXRenderer.class;
            }

            new Client(coordinatorAddress, rendererClass);
        }
    }

}