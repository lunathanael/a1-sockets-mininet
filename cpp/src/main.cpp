#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <chrono>
#include <optional>

void ack(int sockfd) {
    char message[] = "\x06";
    if (send(sockfd, message, sizeof(message), 0) == -1) {
        spdlog::error("send serror");
        exit(1);
    }
}

void runServer(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1)  {
        spdlog::error("error making socket");
        exit(1);
    }

    // Option for allowing you to reuse socket
    int yes {1};
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        spdlog::error("error setsockopt");
        exit(1);
    }

    // Bind socket to a port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sockfd, (sockaddr*)&addr, (socklen_t) sizeof(addr)) == -1) {
        spdlog::error("error binding socket");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(sockfd, 10) == -1) {
        spdlog::error("error listening");
        exit(1);
    }

    spdlog::info("iPerfer server started");

    // Accept one new connection
    struct sockaddr_in connection;
    socklen_t size = sizeof(connection);
    int connectionfd = accept(sockfd, (struct sockaddr*)&connection, &size);

    spdlog::info("Client connected");

    // // Print what IP address connection is from
    // char s[INET6_ADDRSTRLEN];
    // spdlog::debug("connetion from %s\n", inet_ntoa(connection.sin_addr));

    // Initial Recv
    std::optional<decltype(std::chrono::steady_clock::now())> prev;
    decltype(std::chrono::steady_clock::now()-std::chrono::steady_clock::now()) total{0};
    {
        char buf[1024];
        int ret {};
        for(int i = 0; i < 8; ++i) {
            if ((ret = recv(connectionfd, buf, sizeof(buf), 0)) == -1) {
                spdlog::error("recv");
                close(connectionfd);
                return;
            }
            if(prev.has_value()) {
                total += std::chrono::steady_clock::now() - prev.value();
            }
            ack(sockfd);
            prev = std::chrono::steady_clock::now();
        }
    }
    std::chrono::duration propagation_delay = total / 7;
    int bytes_sent{0};
    total = total.zero();

    // Receive messages
    {
        char buf[81920];
        int ret {};

        while ((ret = recv(connectionfd, buf, sizeof(buf), 0)) != -1) {
            total += std::chrono::steady_clock::now() - prev.value() - propagation_delay;
            bytes_sent += ret;
            ack(sockfd);
            prev = std::chrono::steady_clock::now();
        }
    }


    close(connectionfd);
    close(sockfd);

    spdlog::info("Received={} KB, Rate={:.3f} Mbps, RTT={} ms", bytes_sent, static_cast<double>(bytes_sent) / std::chrono::duration_cast<std::chrono::seconds>(total).count(), std::chrono::duration_cast<std::chrono::milliseconds>(propagation_delay).count());
}

int main(int argc, char** argv) {
    cxxopts::Options options("iPerfer", "A simple network performance measurement tool");
    options.add_options()
        ("s, server", "Enable server", cxxopts::value<bool>())
        ("p, port", "Port number to use", cxxopts::value<int>());

    auto result = options.parse(argc, argv);

    auto is_server = result["server"].as<bool>();
    auto port = result["port"].as<int>();

    spdlog::debug("About to check port number...");
    if (port < 1024 || port > 0xFFFF) {
      spdlog::error("Port number should be in interval [1024, 65535]; instead received {}", port); 
      return -1; 
    }

    spdlog::info("Setup complete! Server mode: {}. Listening/sending to port {}", is_server, port);

    
    if(is_server) {
        runServer(port);
    }
    else {

    }
    return 0;
}