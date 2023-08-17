#ifndef API_H
#define API_H

#ifdef _WIN32
#include <conio.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define CLEAR_COMMAND "cls"
#else
#define CLEAR_COMMAND "clear"
#endif

// struct declaration
struct Subscriber {
    char client_id[10];             // identificador do cliente
    char source_id[10];             // identificador da source subscrita
    int credits;                    // creditos disponiveis
    struct sockaddr_in clientAddr;  // informação do cliente
};

struct PDU_1 {
    char identifier[10];                              // identificador de fonte
    int i;                                            // número para criar uma amostra
    int value;                                        // amostra
    int period;                                       // período atual
    int frequency;                                    // frequência
    int multiple;                                     // amostragem
    int max_period;                                   // número máximo de períodos
    std::chrono::system_clock::time_point timestamp;  // timestamp atual
    bool sent;                                        // campo para saber se o PDU foi enviado
};

struct PDU_2 {
    int id;                   // identificador do comando (request do cliente)
    char type[5];             // tipo de request (relacionado com o comando)
    char active_sources[10];  // lista de fontes ativas
    PDU_1 pdu;
    Subscriber sub;
};

struct PDU_3 {
    int n_subscribers;
    int n_sources;
};

// Helper lambda function to print key-value pairs
auto print_key_value = [](const auto& key, const auto& value) {
    std::cout << "Key:[" << key << "] Value:[" << value << "]\n";
};

void create_sender_socket(const std::string ip, int port, int& sockfd, struct sockaddr_in& addr) {
    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return;
    }
    // Set up the adrr address and port
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // std::cout << "port = " << addr.sin_port << std::endl;
    if (inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr)) <= 0) {
        std::cerr << "Invalid server address." << std::endl;
        return;
    }
}

void create_receiver_socket(int port, int& sockfd, struct sockaddr_in& adrr) {
    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return;
    }
    // Set up the address and port
    memset(&adrr, 0, sizeof(adrr));
    adrr.sin_family = AF_INET;
    adrr.sin_port = htons(port);
    adrr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all network interfaces

    if (bind(sockfd, (struct sockaddr*)&adrr, sizeof(adrr)) < 0) {
        std::cerr << "Failed to bind socket." << std::endl;
        close(sockfd);
        return;
    }
}

void print_pdu_1(const PDU_1& pdu) {
    std::cout << "Identifier: " << pdu.identifier << std::endl;
    std::cout << "i: " << pdu.i << std::endl;
    std::cout << "Value: " << pdu.value << std::endl;
    std::cout << "Period: " << pdu.period << std::endl;
    std::cout << "Frequency: " << pdu.frequency << std::endl;
    std::cout << "Multiple: " << pdu.multiple << std::endl;
    std::cout << "Max Period: " << pdu.max_period << std::endl;
    std::time_t timestamp = std::chrono::system_clock::to_time_t(pdu.timestamp);
    std::cout << "Timestamp: " << std::put_time(std::localtime(&timestamp), "%H:%M:%S") << std::endl;
}

void print_pdu_2(const PDU_2& pdu) {
    std::cout << "Identifier: " << pdu.id << std::endl;
    std::cout << "Type: " << pdu.type << std::endl;
    std::cout << "Active sources: " << pdu.active_sources << std::endl;
    std::cout << "Subscriber Id: " << pdu.sub.client_id << std::endl;
    std::cout << "Subscribed source: " << pdu.sub.source_id << std::endl;
    char ip_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pdu.sub.clientAddr.sin_addr, ip_address, INET6_ADDRSTRLEN);
    std::cout << "Subscriber ip: " << ip_address << std::endl;
    unsigned short port = ntohs(pdu.sub.clientAddr.sin_port);
    std::cout << "Subscriber port: " << port << std::endl;
    std::cout << "Subscriber credits: " << pdu.sub.credits << std::endl;
    print_pdu_1(pdu.pdu);
}

bool is_key_pressed() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    return FD_ISSET(STDIN_FILENO, &fds) != 0;
#endif
}

char get_char() {
#ifdef _WIN32
    return _getchar();
#else
    char buf = 0;
    struct termios old;
    fflush(stdout);
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(STDIN_FILENO, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(STDIN_FILENO, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return buf;
#endif
}

#endif