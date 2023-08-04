#include "api.h"

std::mutex screen_mutex;
std::condition_variable cv;

std::atomic_bool confirmation_showing(false);

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
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

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
        if (is_key_pressed() && get_char() == '\n') {  // Check for Enter key
            break;
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
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (is_key_pressed() && get_char() == '\n') {  // Check for Enter key
            break;
        }
    }
}

void display_chooser(std::string &input) {
    std::cout << "------------------------------------" << std::endl;
    std::cout << "           CHOOSE SOURCE            " << std::endl;
    std::cout << "------------------------------------" << std::endl;
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

void still_watching(int period, std::atomic_bool &exit) {
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
                exit.store(true);
                cv.notify_all();
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
    for (size_t i = 0; i < pdu_2.pdu.value; i++) {
        std::cout << "*";
    }
    std::cout << std::endl;
}

void recv_pdu(char *client_id, int type, int sockfd, PDU_2 &pdu_2) {
    sockaddr_in clientAddr = {};
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (true) {
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

void display_channel(PDU_2 &pdu_2, int sockfd, std::string input, char *client_id, std::atomic_bool &exit) {
    while (!exit) {
        {
            std::unique_lock<std::mutex> lock(screen_mutex);
            // Wait if still watching confirmation is using the screen
            cv.wait(lock, [] { return !confirmation_showing; });

            memset(&pdu_2, 0, sizeof(pdu_2));
            recv_pdu(client_id, 0, sockfd, pdu_2);
            if (std::strcmp(pdu_2.sub.source_id, input.c_str()) == 0) {
                display_sin_value(pdu_2);
            }
        }
    }
}

void populate_pdu(PDU_2 &pdu, int id, std::string type, char *client_id, const std::string source_id = "\0", const std::string source_info_id = "\0") {
    size_t length;
    size_t max_size;

    memset(&pdu, 0, sizeof(PDU_2));

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

void menu_handler(int port, int sockfd, struct sockaddr_in serverAddr, char *client_id) {
    int choice;
    bool quit = false;
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
        }

        // Process user's choice
        switch (choice) {
            case 1:  // List all
                system(CLEAR_COMMAND);
                populate_pdu(pdu_2, choice, "list", client_id);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, choice, sockfd, pdu_2);
                display_sources(pdu_2);
                break;
            case 2:  // Info(D)
                system(CLEAR_COMMAND);
                display_chooser(input);
                populate_pdu(pdu_2, choice, "info", client_id, "\0", input);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                recv_pdu(client_id, choice, sockfd, pdu_2);
                system(CLEAR_COMMAND);
                display_info(pdu_2.pdu);
                break;
            case 3:  // Play(D)
            {
                system(CLEAR_COMMAND);
                display_chooser(input);
                populate_pdu(pdu_2, choice, "play", client_id, input, "\0");
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
                system(CLEAR_COMMAND);
                std::atomic<bool> exit(false);
                std::thread display_thread(display_channel, std::ref(pdu_2), sockfd, input, client_id, std::ref(exit));
                std::thread still_watching_thread(still_watching, 10, std::ref(exit));
                display_thread.join();
                still_watching_thread.join();
                if (exit) {
                    populate_pdu(pdu_2, 4, "stop", client_id, "\0", input);
                    if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                        std::cerr << "Failed to send response to client." << std::endl;
                    }
                }

                break;
            }
            case 4:  // Stop(D)
                system(CLEAR_COMMAND);
                display_chooser(input);
                populate_pdu(pdu_2, choice, "stop", client_id, "\0", input);
                if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) == -1) {
                    std::cerr << "Failed to send response to client." << std::endl;
                }
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