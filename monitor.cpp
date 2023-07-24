#include "api.h"

void receive_pdu(int port, PDU_3& pdu) {
    try { /* Continuously listen for incoming PDUs from SM */
        int sockfd;
        struct sockaddr_in serverAddr, clientAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));

        create_receiver_socket(port, sockfd, serverAddr);

        while (true) {
            memset(&clientAddr, 0, sizeof(clientAddr));
            socklen_t clientAddrLen = sizeof(clientAddr);

            ssize_t bytes_read = 0;
            if ((bytes_read = recvfrom(sockfd, &pdu, sizeof(PDU_3), 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) < 0) {
                std::cerr << "Failed to receive data." << std::endl;
                close(sockfd);
                return;
            }
            /* std::cout << "PDU-1: " << std::endl;
            print_pdu_1(pdu); */
        }

        close(sockfd);
    } catch (const std::exception& e) {
        std::cerr << "Exception in received_pdu: " << e.what() << std::endl;
    }
}

void display_data(PDU_3 pdu_3) {
    std::cout << "------------------------------------" << std::endl;
    std::cout << "           SERVER STATUS            " << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Number of subscribers: " << pdu_3.n_subscribers << std::endl;
    std::cout << "Number of sources: " << pdu_3.n_sources << std::endl;
    std::cout << "------------------------------------" << std::endl;
}

void handler() {
    PDU_3 pdu;
    receive_pdu(123654, pdu);
    display_data(pdu);
}

int main() {
    handler();
    return 0;
}