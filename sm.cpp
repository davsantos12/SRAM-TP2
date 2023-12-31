#include "api.h"

std::atomic<bool> keep_running(true);
std::atomic<bool> new_notification(false);

std::mutex sources_mutex;    // Mutex for accessing the list of active pdu's
std::mutex client_mutex;     // Mutex for accessing the list of subscribed clients
std::condition_variable cv;  // Condition variable for signaling between threads

std::unordered_map<std::string, PDU_1> sources_map;
std::unordered_map<in_port_t, Subscriber> subscriber_list;

void receive_pdu(int port) {
    try { /* Continuously listen for incoming PDUs from sources
             Update the list of active sources and signal the main thread
             whenever a new PDU is received and processed */
        int sockfd;
        struct sockaddr_in serverAddr, clientAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));

        create_receiver_socket(port, sockfd, serverAddr);

        while (keep_running.load()) {
            memset(&clientAddr, 0, sizeof(clientAddr));
            socklen_t clientAddrLen = sizeof(clientAddr);

            PDU_1 pdu;
            ssize_t bytes_read = 0;
            if ((bytes_read = recvfrom(sockfd, &pdu, sizeof(PDU_1), 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) < 0) {
                std::cerr << "Failed to receive data." << std::endl;
                close(sockfd);
                return;
            }
            std::string key(pdu.identifier, strlen(pdu.identifier));
            {
                std::lock_guard<std::mutex> lock(sources_mutex);
                sources_map[key] = pdu;
            }
            new_notification.store(true);
            cv.notify_one();
        }
        close(sockfd);
    } catch (const std::exception& e) {
        std::cerr << "Exception in received_pdu: " << e.what() << std::endl;
        keep_running.store(false);
        cv.notify_all();
    }
}

void send_pdu(const std::string ip, int port) {
    try { /*  Continuously check for new PDUs in the list of processed PDUs
              Identify subscribed clients for each PDU and send the PDU to those clients */
        int sockfd;
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));

        create_sender_socket(ip, port, sockfd, serverAddr);

        while (keep_running.load()) {
            {
                std::unordered_set<std::string> sent_pdus;
                std::unique_lock<std::mutex> lock(sources_mutex);
                cv.wait(lock, [] { return !sources_map.empty() && !subscriber_list.empty() && new_notification.load(); });
                std::unique_lock<std::mutex> sub_lock(client_mutex);
                PDU_2 pdu = {};

                for (auto& subscriber : subscriber_list) {
                    auto source = sources_map.find(subscriber.second.source_id);
                    if (source != sources_map.end() && source->second.sent == false && source->second.period != 0 && subscriber.second.credits > 0) {
                        pdu.id = 0;
                        char type[] = "data";
                        size_t length = strlen(type);
                        memcpy(pdu.type, type, length);
                        pdu.type[length] = '\0';
                        pdu.pdu = sources_map[subscriber.second.source_id];
                        subscriber.second.credits -= 1;
                        pdu.sub = subscriber.second;
                        ssize_t bytes_sent = sendto(sockfd, &pdu, sizeof(pdu), 0, (struct sockaddr*)&subscriber.second.clientAddr, sizeof(subscriber.second.clientAddr));
                        if (bytes_sent == -1) {
                            std::cerr << "Failed to send response to client." << std::endl;
                        }
                        sent_pdus.insert(subscriber.second.source_id);
                    }
                }
                sub_lock.unlock();
                for (auto& pdu_id : sent_pdus) {
                    auto source = sources_map.find(pdu_id);
                    if (source != sources_map.end()) {
                        source->second.sent = true;
                    }
                }

                new_notification.store(false);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in send_pdu: " << e.what() << std::endl;
        keep_running.store(false);
        cv.notify_all();
    }
}

void send_monitor_data(const std::string ip, int port) {
    try {
        int sockfd;
        struct sockaddr_in monitorAddr;
        memset(&monitorAddr, 0, sizeof(monitorAddr));

        create_sender_socket(ip, port, sockfd, monitorAddr);

        size_t previous_sources_size = 0;
        size_t previous_subscribers_size = 0;

        while (keep_running.load()) {
            size_t current_sources_size = 0;
            size_t current_subscribers_size = 0;

            {
                std::lock_guard<std::mutex> lock(sources_mutex);
                current_sources_size = sources_map.size();
            }
            {
                std::lock_guard<std::mutex> lock(client_mutex);
                current_subscribers_size = subscriber_list.size();
            }
            if (current_sources_size != previous_sources_size || current_subscribers_size != previous_subscribers_size) {
                PDU_3 pdu_3;
                pdu_3.n_sources = current_sources_size;
                pdu_3.n_subscribers = current_subscribers_size;

                ssize_t bytes_sent = sendto(sockfd, &pdu_3, sizeof(pdu_3), 0, (struct sockaddr*)&monitorAddr, sizeof(monitorAddr));
                if (bytes_sent == -1) {
                    std::cerr << "Failed to send PDU_3 to monitor." << std::endl;
                }
                previous_sources_size = current_sources_size;
                previous_subscribers_size = current_subscribers_size;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        close(sockfd);
    } catch (const std::exception& e) {
        std::cerr << "Exception in send_monitor_data thread: " << e.what() << std::endl;
        keep_running.store(false);
        cv.notify_all();
    }
}

void get_sources_list(PDU_2& pdu_2) {  // Gets list of active sources and stores in pdu_2
    int i = 0;
    int size = sizeof(pdu_2.active_sources);
    {
        std::lock_guard<std::mutex> lock(sources_mutex);
        for (const auto& pdu : sources_map) {
            if (i >= size) {
                break;  // Ensuring we don't exceed the size of active_sources array
            }
            std::strncpy(pdu_2.active_sources + i, pdu.first.c_str(), size - i);
            i += std::min(size - i, static_cast<int>(pdu.first.length()));
        }
    }
    pdu_2.active_sources[i] = '\0';
}

void send_ack(PDU_2& pdu_2, int sockfd) {
    std::string type("ack");
    size_t length = strlen(type.c_str());
    size_t max_size = sizeof(pdu_2.type);
    size_t bytes_sent;
    pdu_2.id = 5;
    if (length < max_size) {
        memcpy(pdu_2.type, type.c_str(), length);
        pdu_2.type[length] = '\0';
    } else {
        std::cerr << "Length of type bigger than allowed." << std::endl;
    }

    if ((bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr*)&pdu_2.sub.clientAddr, sizeof(pdu_2.sub.clientAddr))) < 0) {
        std::cerr << "Failed to send response to client." << std::endl;
    }
}

void process_request(PDU_2 pdu_2, int sockfd, int credits) {  // Process user requests
    ssize_t bytes_sent = 0;
    PDU_1 pdu;
    switch (pdu_2.id) {
        case 1:  // List sources
            // Looks up for sources available
            get_sources_list(pdu_2);
            bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr*)&pdu_2.sub.clientAddr, sizeof(pdu_2.sub.clientAddr));
            if (bytes_sent == -1) {
                std::cerr << "Failed to send response to client." << std::endl;
            }
            break;
        case 2:  // Get source info
            // Looks up for info about the required source
            pdu = {};
            {
                std::lock_guard<std::mutex> lock(sources_mutex);
                if (sources_map.count(pdu_2.pdu.identifier) > 0) {
                    pdu = sources_map[pdu_2.pdu.identifier];
                }
            }
            pdu_2.pdu = pdu;

            bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr*)&pdu_2.sub.clientAddr, sizeof(pdu_2.sub.clientAddr));
            if (bytes_sent == -1) {
                std::cerr << "Failed to send response to client." << std::endl;
            }
            break;
        case 3:  // Play from source
            // Adds subscriber to subscriber list and notifies sender thread to send to new sub.
            {
                std::unique_lock<std::mutex> source_lock(sources_mutex);
                if (sources_map.count(pdu_2.sub.source_id) > 0) {
                    pdu_2.sub.credits = 100;
                    std::unique_lock<std::mutex> sub_lock(client_mutex);
                    if (subscriber_list.count(pdu_2.sub.clientAddr.sin_port) > 0) {
                        subscriber_list[pdu_2.sub.clientAddr.sin_port].credits = 100;
                        send_ack(pdu_2, sockfd);
                        sub_lock.unlock();
                        source_lock.unlock();
                    } else {
                        subscriber_list[pdu_2.sub.clientAddr.sin_port] = pdu_2.sub;
                        send_ack(pdu_2, sockfd);
                        sub_lock.unlock();
                        source_lock.unlock();
                    }
                    cv.notify_one();
                }
            }
            break;
        case 4:  // Stop playing from source
            // Removes subscribers
            {
                std::unique_lock<std::mutex> lock(client_mutex);
                if (subscriber_list.count(pdu_2.sub.clientAddr.sin_port) > 0) {
                    subscriber_list.erase(pdu_2.sub.clientAddr.sin_port);
                    send_ack(pdu_2, sockfd);
                    lock.unlock();
                }
            }
            break;
        case 6:
            // Get subscribed sources
            {
                std::unique_lock<std::mutex> lock(client_mutex);
                auto subscriber = subscriber_list.find(pdu_2.sub.clientAddr.sin_port);
                if (subscriber != subscriber_list.end()) {
                    pdu_2.sub = subscriber->second;
                    bytes_sent = sendto(sockfd, &pdu_2, sizeof(pdu_2), 0, (struct sockaddr*)&pdu_2.sub.clientAddr, sizeof(pdu_2.sub.clientAddr));
                    if (bytes_sent == -1) {
                        std::cerr << "Failed to send response to client." << std::endl;
                    }
                }
                lock.unlock();
            }
            break;
        default:
            std::cerr << "Invalid request from client." << std::endl;
    }
}

void manage_client_requests(const std::string ip, int port, int credits) {
    try { /*  Listen for client commands (e.g., list, info(D), play(D), stop(D))
          Update the list of subscribed clients based on commands received */
        int sockfd;
        struct sockaddr_in serverAddr, clientAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));

        create_receiver_socket(port, sockfd, serverAddr);
        // std::cout << "socket created with port = " << serverAddr.sin_port << std::endl;

        while (keep_running.load()) {
            memset(&clientAddr, 0, sizeof(clientAddr));
            socklen_t clientAddrLen = sizeof(clientAddr);
            PDU_2 pdu_2;
            ssize_t bytes_received = recvfrom(sockfd, &pdu_2, sizeof(PDU_2), 0, (struct sockaddr*)&clientAddr, &clientAddrLen);

            if (bytes_received == -1) {
                std::cerr << "Error receiving client request." << std::endl;
                continue;
            }
            memcpy(&pdu_2.sub.clientAddr, &clientAddr, sizeof(clientAddr));
            // print_pdu_2(pdu_2);
            process_request(pdu_2, sockfd, credits);
        }
        close(sockfd);
    } catch (const std::exception& e) {
        std::cerr << "Exception in manage_client_requests: " << e.what() << std::endl;
        keep_running.store(false);
        cv.notify_all();
    }
}

void cleanup_thread(int period) {
    try {
        while (keep_running.load()) {
            std::chrono::microseconds tolerance(200);
            std::vector<std::string> sources_id;
            std::vector<in_port_t> sub_id;
            {
                std::lock_guard<std::mutex> lock(sources_mutex);
                if (!sources_map.empty()) {
                    for (const auto& pdu : sources_map) {
                        std::chrono::microseconds pdu_period = std::chrono::microseconds(1000000 / (pdu.second.frequency * pdu.second.multiple));
                        if (std::chrono::system_clock::now() - pdu.second.timestamp > pdu_period + tolerance) {
                            std::string source_id = pdu.second.identifier;
                            sources_id.push_back(source_id);
                        }
                    }
                    for (const auto& id : sources_id) {
                        sources_map.erase(id);
                    }
                }
            }
            {
                std::lock_guard<std::mutex> lock(client_mutex);
                if (!subscriber_list.empty()) {
                    for (const auto& sub : subscriber_list) {
                        if (sub.second.credits == 0) {
                            sub_id.push_back(sub.first);
                        }
                    }
                    for (const auto& id : sub_id) {
                        subscriber_list.erase(id);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(period));
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in cleanup_thread: " << e.what() << std::endl;
        keep_running.store(false);
        cv.notify_all();
    }
}

int main() {
    try {
        std::thread receiver_thread(receive_pdu, 12345);
        std::thread sender_thread(send_pdu, "127.0.0.1", 12347);
        std::thread manager_thread(manage_client_requests, "127.0.0.1", 12347, 100);
        std::thread monitor_thread(send_monitor_data, "127.0.0.1", 12365);
        std::thread cleaner_thread(cleanup_thread, 1);

        receiver_thread.join();
        sender_thread.join();
        manager_thread.join();
        monitor_thread.join();
        cleaner_thread.join();
    } catch (const std::exception& e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        keep_running.store(false);
    }

    return 0;
}