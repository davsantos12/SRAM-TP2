#include "api.h"

void display_data(PDU_3 pdu_3) {
    std::cout << "------------------------------------" << std::endl;
    std::cout << "           SERVER STATUS            " << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Number of subscribers: " << pdu_3.n_subscribers << std::endl;
    std::cout << "Number of sources: " << pdu_3.n_sources << std::endl;
    std::cout << "------------------------------------" << std::endl;
}

void receive_pdu(int port) {
    try { /* Continuously listen for incoming PDUs from SM */
        int sockfd;
        struct sockaddr_in monitorAddr;
        memset(&monitorAddr, 0, sizeof(monitorAddr));

        create_receiver_socket(port, sockfd, monitorAddr);
        socklen_t monitorAddrLen = sizeof(monitorAddr);
        PDU_3 pdu;

        while (true) {
            ssize_t bytes_read = 0;
            if ((bytes_read = recvfrom(sockfd, &pdu, sizeof(PDU_3), 0, (struct sockaddr*)&monitorAddr, &monitorAddrLen)) < 0) {
                std::cerr << "Failed to receive data." << std::endl;
                close(sockfd);
                return;
            }
            system(CLEAR_COMMAND);
            display_data(pdu);
        }

        close(sockfd);
    } catch (const std::exception& e) {
        std::cerr << "Exception in received_pdu: " << e.what() << std::endl;
    }
}

void handler() {
    receive_pdu(12365);
}

int main() {
    handler();
    return 0;
}