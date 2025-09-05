#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

void severRunner(int port) {
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

    // // Receive message
    // char buf[1024];
    // int ret {};
    // if ((ret = recv(connectionfd, buf, sizeof(buf), 0)) == -1) {
    //     perror("recv");
    //     close(connectionfd);
    //     continue;
    // }
    // buf[ret] = '\0';

    // printf("message: %s\n", buf);


    close(connectionfd);
    close(sockfd);
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

    }
    else {

    }
    return 0;
}