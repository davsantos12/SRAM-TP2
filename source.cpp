#include "api.h"

int generate_sample(int i, int N) {
    return static_cast<int>(1 + (1 + sin(2 * M_PI * i / N)) * 30);
}

PDU_1 generate_pdu(char* id, int i, int P, int F, int N, int M) {
    PDU_1 pdu;
    size_t length = strlen(id);
    size_t max_size = sizeof(pdu.identifier);
    if (length < max_size - 1) {
        memcpy(pdu.identifier, id, length);
        pdu.identifier[length] = '\0';
    }
    pdu.i = i;
    if (P == 0) {
        pdu.value = 0;
    } else {
        pdu.value = generate_sample(i, N);
    }
    pdu.frequency = F;
    pdu.multiple = N;
    pdu.period = P;
    pdu.max_period = M;
    pdu.sent = false;
    pdu.timestamp = std::chrono::system_clock::now();

    return pdu;
}

void read_config_file(std::string filename, std::string& IP, int& F, int& N, int& M, int& port) {
    std::ifstream input_file(filename);
    if (!input_file) {
        std::cout << "Failed to open the file." << std::endl;
        return;
    }

    if (input_file >> F >> N >> M >> IP >> port) {
        // File reading was successful
        std::cout << "Config file loaded with success." << std::endl;
    } else {
        std::cout << "Failed to read the values from the file." << std::endl;
    }

    input_file.close();
}

void handler(std::string filename, char* D) {
    std::string IP;
    bool first_iteration = true;
    int F, N, M, port, sockfd;
    struct sockaddr_in server;

    read_config_file(filename, IP, F, N, M, port);

    create_sender_socket(IP, port, sockfd, server);

    int Fa = F * N;
    while (true) {
        int start_p = first_iteration ? 0 : 1;
        for (int P = start_p; P <= M; P++) {
            for (int i = 0; i < Fa; i++) {
                PDU_1 pdu = generate_pdu(D, i, P, F, N, M);
                print_pdu_1(pdu);
                std::cout << std::endl;
                int sent_bytes = sendto(sockfd, &pdu, sizeof(pdu), 0, (struct sockaddr*)&server, sizeof(server));
                std::this_thread::sleep_for(std::chrono::microseconds(1000000 / (F * N)));
            }
        }
        first_iteration = false;
    }
    close(sockfd);
}

int main(int argc, char* argv[]) {
    std::filesystem::path program_path(argv[0]);
    std::string program_name = program_path.filename().string();
    char* program_name_ptr = new char[program_name.length() + 1];
    std::strcpy(program_name_ptr, program_name.c_str());
    handler(argv[1], program_name_ptr);
    delete[] program_name_ptr;
    return 0;
}