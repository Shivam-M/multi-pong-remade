#include "client.h"
#include "coordinator.h"
#include "server.h"
#include "tools/logger.h"
#include "tools/common.h"
#include "tools/renderer.h"
#include "tools/renderer_opengl.h"

#ifdef _WIN32
#include "tools/renderer_directx11.h"
#endif

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <memory>

struct Arguments {
	bool invalid = false;
	bool client = true;
	bool server = false;
	bool coordinator = false;
	bool directx_11 = false;
	std::optional<int> port;
	std::optional<std::string> host;
	std::vector<std::pair<std::string, int>> server_addresses;
	Logger::Level log_level = Logger::Level::Info;
};

static std::optional<std::pair<std::string, int>> parse_address(const std::string& address) {
	auto colon = address.find(':');
	if (colon == std::string::npos) return std::nullopt;

	std::string host = address.substr(0, colon);
	int port = 0;

	try {
		port = std::stoi(address.substr(colon + 1));
		if (port < 1 || port > 65535) return std::nullopt;
	}
	catch (...) {
		return std::nullopt;
	}

	return std::make_pair(host, port);
}

static Arguments parse_arguments(int argc, char** argv) {
	Arguments arguments;

	if (argc <= 1) return arguments;

	arguments.invalid = true;

	for (int i = 1; i < argc; i++) {
		std::string argument = argv[i];

		if (argument == "--client") {
			arguments.client = true;
		} else if (argument == "--server") {
			arguments.server = true;
		} else if (argument == "--coordinator") {
			arguments.coordinator = true;
		} else if (argument == "--directx11" || argument == "--dx11") {
			arguments.directx_11 = true;
		} else if (argument == "--verbose") {
			arguments.log_level = Logger::Level::Debug;
		} else if (argument == "--host") {
			if (i + 1 < argc) {
				arguments.host = argv[++i];
			} else {
				Logger::error("Specify a valid IP address with --host <address>");
				return arguments;
			}
		} else if (argument == "--port") {
			if (i + 1 < argc) {
				try {
					arguments.port = std::stoi(argv[++i]);
					if (arguments.port < 1 || arguments.port > 65535) {
						Logger::error("Specify a valid port number with --port <1-65535>");
						return arguments;
					}
				} catch (...) {
					Logger::error("Specify a valid port number with --port <1-65535>");
					return arguments;
				}
			} else {
				Logger::error("Specify a valid port number with --port <1-65535>");
				return arguments;
			}
		} else if (argument == "--server-address") {
			if (i + 1 < argc) {
				if (auto address = parse_address(argv[++i])) {
					arguments.server_addresses.push_back(*address);
				} else {
					Logger::error("Invalid server address: ", argv[i - 1]);
					return arguments;
				}
			} else {
				Logger::error("Specify multiple server addresses with --server-address <address:port>");
				return arguments;
			}
		} else if (argument == "--help") {
			std::cout <<
				"usage: " << argv[0] << " [options]\n\n"
				"options:\n"
				"  --client                      <default>\n"
				"  --server                      run server\n"
				"  --coordinator                 run coordinator\n"
				"  --host <address>              [client] address of the coordinator\n"
				"  --port <1-65535>              [client] port of the coordinator\n"
				"                                [server/coordinator] port to listen on\n"
				"  --server-address <host:port>  [coordinator] (multiple) game server endpoints\n"
				"  --verbose                     enable debug logging\n"
				"  --help                        show help\n";
			return arguments;
		}
	}

	arguments.invalid = false;
	return arguments;
}

int main(int argc, char** argv) {
	Arguments arguments = parse_arguments(argc, argv);

	Logger::level = arguments.log_level;

	if (arguments.invalid) {
		return -1;
	}

	if (arguments.server && arguments.coordinator) {
		Logger::warning("Both --server and --coordinator specified - running the server");
	}

	if (arguments.server) {
		int port = arguments.port.value_or(MULTI_PONG_SERVER_PORT);

		Server server = Server(port);
		return 0;
	}

	if (arguments.coordinator) {
		int port = arguments.port.value_or(MULTI_PONG_COORDINATOR_PORT);

		Coordinator coordinator = Coordinator(port, arguments.server_addresses);
		return 0;
	}

	if (arguments.client) {
		std::string address = arguments.host.value_or(MULTI_PONG_COORDINATOR_ADDRESS.first);
		int port = arguments.port.value_or(MULTI_PONG_COORDINATOR_ADDRESS.second);

		std::unique_ptr<Renderer> renderer;

#ifdef _WIN32
		if (arguments.directx_11) {
			renderer = std::make_unique<DirectX11Renderer>();
		} else {
			renderer = std::make_unique<OpenGLRenderer>();
		}
#else
		if (arguments.directx_11) {
			Logger::warning("DirectX 11 is not supported on this platform - falling back to the OpenGL renderer");
		}
		renderer = std::make_unique<OpenGLRenderer>();
#endif

		Client client = Client(address, port, std::move(renderer));
		return 0;
	}
	
	return 0;
}
