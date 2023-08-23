#include "api.h"

std::mutex screen_mutex;
std::condition_variable cv;

std::atomic_bool confirmation_showing(false);

void populate_pdu(PDU_2 &pdu, int id, std::string type, char *client_id, const std::string source_id = "\0", const std::string source_info_id = "\0") {
    size_t length;
    size_t max_size;

    pdu = {};

    pdu.id = id;
    length = strlen(type.c_str());
    max_size = sizeof(pdu.type);
    if (length < max_size) {
        memcpy(pdu.type, type.c_str(), length);
        pdu.type[length] = '\0';
    } else {
        std::cerr << "Length of type bigger than allowed." << std::endl;
    }
    length = strlen(client_id);
    max_size = sizeof(pdu.sub.client_id);
    if (length < max_size) {
        memcpy(pdu.sub.client_id, client_id, length);
        pdu.sub.client_id[length] = '\0';
    } else {
        std::cerr << "Length of client_id bigger than allowed." << std::endl;
    }
    length = strlen(source_id.c_str());
    max_size = sizeof(pdu.sub.source_id);
    if (length < max_size) {
        memcpy(pdu.sub.source_id, source_id.c_str(), length);
        pdu.sub.source_id[length] = '\0';
    } else {
        std::cerr << "Length of source_id bigger than allowed." << std::endl;
    }
    length = strlen(source_info_id.c_str());
    max_size = sizeof(pdu.pdu.identifier);
    if (length < max_size) {
        memcpy(pdu.pdu.identifier, source_info_id.c_str(), length);
        pdu.pdu.identifier[length] = '\0';
    } else {
        std::cerr << "Length of source_info_id bigger than allowed." << std::endl;
    }
}

void display_menu() {
    std::cout << "------------------------------------" << std::endl;
    std::cout << "            MENU OPTIONS            " << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout << "1. List available sources" << std::endl;
    std::cout << "2. Get more info on sources" << std::endl;
    std::cout << "3. Play from a source" << std::endl;
    std::cout << "4. Stop playing from a source" << std::endl;
    std::cout << "5. Quit" << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Enter your choice: ";
}

void display_info(PDU_1 pdu_1) {
    if (pdu_1.identifier[0] != '\0') {
        std::cout << "------------------------------------" << std::endl;
        std::cout << "            INFORMATION             " << std::endl;
        std::cout << "------------------------------------" << std::endl;
        std::cout << "Identifier: " << pdu_1.identifier << std::endl;
        std::cout << "Frequency: " << pdu_1.frequency << std::endl;
        std::cout << "Multiple: " << pdu_1.multiple << std::endl;
        std::cout << "Max Period: " << pdu_1.max_period << std::endl;
        std::cout << "------------------------------------" << std::endl;
        std::cout << "Press ENTER to return.";
        std::cout.flush();

        while (true) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (is_key_pressed() && get_char() == '\n') {  // Check for Enter key
                break;
            }
        }
    }
}

void display_sources(PDU_2 pdu_2) {
    std::cout << "------------------------------------" << std::endl;
    std::cout << "            LIST SOURCES            " << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Sources: ";
    for (size_t i = 0; i < strlen(pdu_2.active_sources); i++) {
        std::cout << pdu_2.active_sources[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Press ENTER to return.";
    std::cout.flush();

    while (true) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        if (is_key_pressed() && get_char() == '\n') {  // Check for Enter key
            break;
        }
    }
}

void display_chooser(std::string &input, PDU_2 pdu_2) {
    std::cout << "------------------------------------" << std::endl;
    std::cout << "           CHOOSE SOURCE            " << std::endl;
    std::cout << "------------------------------------" << std::endl;
    if (pdu_2.id != 6) {
        std::cout << "Available Sources: ";
        for (size_t i = 0; i < strlen(pdu_2.active_sources); i++) {
            std::cout << pdu_2.active_sources[i] << " ";
        }
    } else {
        std::cout << "Subscribed Sources: ";
        for (size_t i = 0; i < strlen(pdu_2.sub.source_id); i++) {
            std::cout << pdu_2.sub.source_id[i] << " ";
        }
    }
    std::cout << std::endl;
    std::cout << "Enter source identifier: ";
    std::cin >> input;
    std::cin.clear();
    std::cout << "------------------------------------" << std::endl;
    std::cout.flush();
}

void display_confirmation() {
    std::cout << "------------------------------------" << std::endl;
    std::cout << "      ARE YOU STILL WATCHING?       " << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout << "                                    " << std::endl;
    std::cout << "      Press ENTER to continue.      " << std::endl;
    std::cout << "                                    " << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout.flush();
}

void recv_pdu(char *client_id, int type, int sockfd, PDU_2 &pdu_2, std::atomic_bool &exit) {
    sockaddr_in clientAddr = {};
    socklen_t clientAddrLen = sizeof(clientAddr);

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(sockfd, &read_set);

    struct timeval timeout;
    timeout.tv_sec = 5;  // Timeout value in seconds
    timeout.tv_usec = 0;

    while (true) {
        int select_res = select(sockfd + 1, &read_set, nullptr, nullptr, &timeout);
        if (select_res == -1) {
            std::cerr << "Failed in socket select" << std::endl;
            close(sockfd);
            break;
        } else if (select_res == 0) {
            // std::cout << "Timeout occurred." << std::endl;
            exit.store(true);
            break;
        }
        ssize_t recvd_bytes = recvfrom(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (recvd_bytes == -1) {
            std::cerr << "Failed to receive response from server" << std::endl;
            break;
        }
        if (std::strcmp(pdu_2.sub.client_id, client_id) == 0 && pdu_2.id == type) {
            break;
        }
    }
}

void still_watching(int period, std::atomic_bool &exit, PDU_2 &pdu_2, char *client_id, std::string input, int sockfd, sockaddr_in serverAddr, ssize_t bytes_sent) {
    auto last_time = std::chrono::steady_clock::now();
    while (!exit) {
        if (std::chrono::steady_clock::now() - std::chrono::seconds(period) >= last_time) {
            last_time = std::chrono::steady_clock::now();
            bool stop = true;
            confirmation_showing.store(true);
            {
                std::unique_lock<std::mutex> lock(screen_mutex);
                system(CLEAR_COMMAND);
                display_confirmation();
                for (size_t seconds = 0; seconds < 10; seconds++) {
                    if (is_key_pressed() && get_char() == '\n') {
                        stop = false;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                system(CLEAR_COMMAND);
            }
            if (stop) {
                populate_pdu(pdu_2, 4, "stop", client_id, "\0", input);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                // recv_pdu(client_id, 5, sockfd, pdu_2);
                exit.store(true);
            }
            confirmation_showing.store(false);
            cv.notify_all();
        }
        if (is_key_pressed() && get_char() == 'q') {  // Check for q key
            exit.store(true);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void display_sin_value(PDU_2 pdu_2) {
    for (int i = 0; i < pdu_2.pdu.value; i++) {
        std::cout << "*";
    }
    std::cout << std::endl;
}

void display_channel(PDU_2 &pdu_2, int sockfd, std::string input, char *client_id, std::atomic_bool &exit, sockaddr_in serverAddr, ssize_t bytes_sent) {
    while (!exit) {
        pdu_2 = {};
        recv_pdu(client_id, 0, sockfd, pdu_2, exit);
        if (std::strcmp(pdu_2.sub.source_id, input.c_str()) == 0) {
            {
                std::unique_lock<std::mutex> lock(screen_mutex);
                // Wait if still watching confirmation is using the screen
                cv.wait(lock, [] { return !confirmation_showing; });
                display_sin_value(pdu_2);
            }
            if (pdu_2.sub.credits < 3) {
                populate_pdu(pdu_2, 3, "play", client_id, input, "\0");
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, 5, sockfd, pdu_2, exit);
            }
        }
    }
}

void menu_handler(int port, int sockfd, struct sockaddr_in serverAddr, char *client_id) {
    int choice;
    bool quit = false;
    std::atomic<bool> exit(false);
    PDU_2 pdu_2;
    std::string input;
    ssize_t bytes_sent = 0;

    while (!quit) {
        // Clear the screen
        system(CLEAR_COMMAND);

        display_menu();
        std::cin >> choice;
        std::cin.clear();

        // Validate user input
        while (std::cin.fail() || choice < 1 || choice > 5) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input, please try again: ";
            std::cin >> choice;
            std::cin.clear();
        }

        // Process user's choice
        switch (choice) {
            case 1:  // List all
                system(CLEAR_COMMAND);
                populate_pdu(pdu_2, choice, "list", client_id);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, choice, sockfd, pdu_2, exit);
                display_sources(pdu_2);
                break;
            case 2:  // Info(D)
                system(CLEAR_COMMAND);
                populate_pdu(pdu_2, 1, "list", client_id);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, 1, sockfd, pdu_2, exit);
                display_chooser(input, pdu_2);
                populate_pdu(pdu_2, choice, "info", client_id, "\0", input);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, choice, sockfd, pdu_2, exit);
                system(CLEAR_COMMAND);
                display_info(pdu_2.pdu);
                break;
            case 3:  // Play(D)
            {
                exit.store(false);
                system(CLEAR_COMMAND);
                populate_pdu(pdu_2, 1, "list", client_id);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, 1, sockfd, pdu_2, exit);
                display_chooser(input, pdu_2);
                populate_pdu(pdu_2, choice, "play", client_id, input, "\0");
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, 5, sockfd, pdu_2, exit);
                system(CLEAR_COMMAND);
                std::thread display_thread(display_channel, std::ref(pdu_2), sockfd, input, client_id, std::ref(exit), serverAddr, bytes_sent);
                std::thread still_watching_thread(still_watching, 40, std::ref(exit), std::ref(pdu_2), client_id, input, sockfd, serverAddr, bytes_sent);
                display_thread.join();
                still_watching_thread.join();
                break;
            }
            case 4:  // Stop(D)
                system(CLEAR_COMMAND);
                populate_pdu(pdu_2, 6, "subd", client_id);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, 6, sockfd, pdu_2, exit);
                display_chooser(input, pdu_2);
                populate_pdu(pdu_2, choice, "stop", client_id, "\0", input);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, 5, sockfd, pdu_2, exit);
                break;
            case 5:  // Quit
                quit = true;
                break;
        }
    }
}

void handler(const std::string ip, int port, char *client_id) {
    int sockfd;
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));

    create_sender_socket(ip, port, sockfd, serverAddr);

    menu_handler(port, sockfd, serverAddr, client_id);

    close(sockfd);

    system(CLEAR_COMMAND);
}

int main(int argc, char *argv[]) {
    std::filesystem::path program_path(argv[0]);
    std::string program_name = program_path.filename().string();
    char *program_name_ptr = new char[program_name.length() + 1];
    std::strcpy(program_name_ptr, program_name.c_str());

    handler("127.0.0.1", 12347, program_name_ptr);

    delete[] program_name_ptr;
    return 0;
}