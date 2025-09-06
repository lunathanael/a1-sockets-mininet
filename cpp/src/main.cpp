#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <chrono>
#include <optional>
#include <string>
#include <ratio>

using hrc = std::chrono::high_resolution_clock;

int ack(int connectionfd) {
    char message[1] = {'A'};
    return send(connectionfd, message, sizeof(message), 0);
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
    if (listen(sockfd, 1) == -1) {
        spdlog::error("error listening");
        exit(1);
    }

    spdlog::info("iPerfer server started");

    // Accept one new connection
    struct sockaddr_in connection;
    socklen_t size = sizeof(connection);
    int connectionfd = accept(sockfd, (struct sockaddr*)&connection, &size);

    spdlog::info("Client connected");

    // Initial Recv
    auto prev = hrc::now();
    std::chrono::nanoseconds total{};
    {
        char buf[1];
        int ret {};
        for(int i = 0; i < 8; ++i) {
            int loc{0};
            while(loc < sizeof(buf)) {
                if ((ret = recv(connectionfd, buf, sizeof(buf), 0)) == -1) {
                    spdlog::error("recv");
                    close(connectionfd);
                    close(sockfd);
                    exit(1);
                }
                loc += ret;
            }
            if(i >= 4) {
                total += hrc::now() - prev;
            }
            if(ack(connectionfd) == -1) {
                spdlog::error("send");
                close(connectionfd);
                close(sockfd);
                exit(1);
            }
            prev = hrc::now();
        }
    }
    auto rtt = std::chrono::nanoseconds(total.count() / 4);
    long long bytes_received{0};

    // Receive messages
    {
        char buf[80 * 1000];
        int ret {};

        auto start = hrc::now();
        int cnt{0};

        while (true) {
            int loc{0};
            while(loc < sizeof(buf)) {
                if((ret = recv(connectionfd, buf + loc, sizeof(buf) - loc, 0)) == -1) {
                    break;
                }
                if(ret == 0)
                    break;
                loc += ret;
            }
            if(loc == 0)
                break;
            ++cnt;
            bytes_received += loc;

            if(ack(connectionfd) == -1) {
                break;
            }
        }

        total = (hrc::now() - start) - (rtt * cnt);
    }

    close(connectionfd);
    close(sockfd);

    double mb_sent = static_cast<double>(bytes_received) / (1000.0 * 1000.0);
    double seconds = std::chrono::duration<double>(total).count();
    double mbps = (seconds > 0) ? (mb_sent * 8.0 / seconds) : 0.0;
    int rtt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(rtt).count();
    
    spdlog::info("Sent={} KB, Rate={:.3f} Mbps, RTT={} ms", bytes_received / 1000, mbps, rtt_ms);
}

void runClient(std::string& hostname, int port, double time) {
    // Make a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1)  {
        perror("error making socket");
        exit(1);
    }


    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    struct hostent *host = gethostbyname(hostname.data());
    if (host == NULL) {
        perror("error gethostbyname");
        exit(1);
    }
    memcpy(&addr.sin_addr, host->h_addr, host->h_length);
    addr.sin_port = htons(port);  // server port

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("error connecting");
        exit(1);
    }

    auto start = hrc::now();
    std::chrono::nanoseconds total{};
    {
        char buf[1];
        char message[1] = {'M'};
        for(int i = 0; i < 8; ++i) {
            if ((send(sockfd, message, sizeof(message), 0)) == -1) {
                spdlog::error("send");
                close(sockfd);
                return;
            }
            start = hrc::now();

            int loc{0};
            while(loc < 1) {
                int res = recv(sockfd, buf, sizeof(buf), 0);
                if (res == -1) {
                    spdlog::error("recv");
                    close(sockfd);
                    exit(1);
                }
                loc += res;
            }
            
            if(i >= 4) {
                total += (hrc::now() - start);
            }
        }
    }

    auto rtt = std::chrono::nanoseconds(total.count() / 4);
    long long bytes_sent{0};

    {
        char message[80 * 1000] = {0};
        char buf[1];
        int result{};

        int cnt{0};
        start = hrc::now();

        while((hrc::now() - start) < std::chrono::duration<double>(time)) {
            if ((result = send(sockfd, message, sizeof(message), 0)) == -1) {
                spdlog::error("send");
                close(sockfd);
                exit(1);
            }


            int loc{0};
            while(loc < 1) {
                int res = recv(sockfd, buf, sizeof(buf), 0);
                if (res == -1) {
                    spdlog::error("recv");
                    close(sockfd);
                    exit(1);
                }
                loc += res;
            }

            bytes_sent += result;
            ++cnt;
        }

        total = (hrc::now() - start) - (rtt * cnt);
    }

    close(sockfd);
    shutdown(sockfd, SHUT_RDWR);

    double mb_sent = static_cast<double>(bytes_sent) / (1000.0 * 1000.0);
    double seconds = std::chrono::duration<double>(total).count();
    double mbps = (seconds > 0) ? (mb_sent * 8.0 / seconds) : 0.0;
    int rtt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(rtt).count();
    
    spdlog::info("Sent={} KB, Rate={:.3f} Mbps, RTT={} ms", bytes_sent / 1000, mbps, rtt_ms);
}

int main(int argc, char** argv) {
    cxxopts::Options options("iPerfer", "A simple network performance measurement tool");
    options.add_options()
        ("s, server", "Enable server", cxxopts::value<bool>())
        ("c, client", "Enable client", cxxopts::value<bool>())
        ("p, port", "Port number to use", cxxopts::value<int>())
        ("h, hostname", "Sever hostname", cxxopts::value<std::string>())
        ("t, time", "Duration in seconds for which data should be generated.", cxxopts::value<double>());

    auto result = options.parse(argc, argv);

    auto is_server = result["server"].as<bool>();
    auto is_client = result["client"].as<bool>();
    auto port = result["port"].as<int>();

    if (is_server == is_client) {
        spdlog::error("Please choose one of either server or client."); 
        return -1; 
    }
    

    spdlog::debug("About to check port number...");
    if (port < 1024 || port > 0xFFFF) {
      spdlog::error("Port number should be in interval [1024, 65535]; instead received {}", port); 
      return -1; 
    }

    spdlog::info("Setup complete! Server mode: {}. Listening/sending to port {}", is_server, port);

    
    if(is_server) {
        spdlog::info("Running server with options, port: {}", port);
        runServer(port);
    }
    else {
        auto time = result["time"].as<double>();
        auto hostname = result["hostname"].as<std::string>();
        if(is_client && time <= 0) {
            spdlog::error("Error: time argument must be greater than 0");
            return -1;
        }
        spdlog::info("Running Client with options, port: {} hostname: {}, time: {}", port, hostname, time);
        runClient(hostname, port, time);
    }
    return 0;
}