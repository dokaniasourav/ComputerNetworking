#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <ctime>
#include <random>

extern int errno;

#define FILE_SIZE 512
#define NUM_OF_RETRIES 10

#define OPCODE_RRQ 1
#define OPCODE_WRQ 2
#define OPCODE_DATA 3
#define OPCODE_ACK 4
#define OPCODE_ERR 5

#define ASCII_MODE 1
#define OCTET_MODE 2

struct tftp_msg_default{
    uint16_t uiOpcode;
};

struct tftp_request_message{
    uint16_t uiOpcode;
    uint8_t uiFileNameMode[FILE_SIZE];
};

struct tftp_ack_message{
    uint16_t uiOpcode;
    uint16_t uiBlockNumber;
};

struct tftp_data_message{
    uint16_t uiOpcode;
    uint16_t uiBlockNumber;
    uint8_t data[FILE_SIZE];
};

struct tftp_error_message{
    uint16_t uiOpcode;
    uint16_t uiErrorCode;
    uint8_t uiErrorData[FILE_SIZE];
};

union tftp_msg{
    tftp_msg_default msgDefault;
    tftp_request_message requestMessage;
    tftp_data_message dataMessage;
    tftp_ack_message ackMessage;
    tftp_error_message errorMessage;
};

int setup_server(int argc, char **argv);

ssize_t send_data(int write_fd, uint16_t type, uint16_t uiBlockNumber, uint8_t data[], size_t message_length,
                  struct sockaddr_in* address, socklen_t* socket_len);
ssize_t receive_data(int client_socket, tftp_msg* msg, sockaddr_in* client_address, socklen_t* client_length);
void process_request(tftp_msg message, ssize_t length_of_msg, sockaddr_in* client_socket, socklen_t* socket_length);


int main(int argc, char *argv[])
{
    sockaddr_in client_address{};
    memset((char *)&client_address, 0, sizeof(client_address));
    socklen_t client_length = sizeof(client_address);
    int server_socket = setup_server(argc, argv);
    int max_sd = server_socket;
    uint16_t received_len, opcode_received;
    char temp_msg[FILE_SIZE];

    fd_set all_sockets, ready_sockets;
    FD_ZERO(&all_sockets);
    FD_SET(server_socket, &all_sockets);

    tftp_msg message{};
                                            // Now start listening for new requests
    while (true)
    {
        ready_sockets = all_sockets;
        timeval wait_time = {0, 10000000};
        int select_ret = select(max_sd+1, &ready_sockets, nullptr, nullptr, &wait_time);

        if ((select_ret < 0) && (errno != EINTR)) {
            printf("Error selecting sockets, %s \n", strerror(errno));
            exit(1);
        }
        if(FD_ISSET(server_socket, &ready_sockets)) {
            received_len = receive_data(server_socket, &message, &client_address, &client_length);
            if (received_len < 0) {
                printf("Zero length data received \n");
                continue;
            } else if (received_len < 4) {
                send_data(server_socket, OPCODE_ERR, 0, (uint8_t *) "Invalid size", strlen(temp_msg), &client_address,
                          &client_length);
                continue;
            }
            opcode_received = ntohs(message.msgDefault.uiOpcode);

            if (opcode_received == OPCODE_RRQ || opcode_received == OPCODE_WRQ) {
                if (fork() == 0) {
                    process_request(message, received_len, &client_address, &client_length);
                    exit(-1);
                }
            } else {
                sprintf(temp_msg, "Invalid opcode received, val = %d; please try again", opcode_received);
                printf("%s, from:%s:%d \n", temp_msg, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                send_data(server_socket, OPCODE_ERR, 0, (uint8_t *) temp_msg, strlen(temp_msg), &client_address, &client_length);
            }
        } else {
            //printf("..\n");
        }
    }
    close(server_socket);
    printf("Goodbye!\n");
    return 0;
}

int setup_server(int argc, char **argv) {
    if (argc != 3) {
        printf("Enter the IP_Address and Port_number as arguments, example: ./server 127.0.0.1 13579 \n");
        exit(1);
    }

    sockaddr_in ipv4{};
    sockaddr_in6 ipv6{};

    addrinfo type_of_addr{}, *temp = nullptr;
    type_of_addr.ai_family = PF_UNSPEC;
    type_of_addr.ai_flags = AI_NUMERICHOST;

    // Figure out whether the address is IPv4 or IPv6 and the call the APIs accordingly
    if (getaddrinfo(argv[1], nullptr, &type_of_addr, &temp)) {
        printf("Invalid address entered %s", argv[1]);
        exit(1);
    }

    int socket_type = -1;
    if(temp != nullptr)
        socket_type = temp->ai_family;

    int socket_desc = socket(socket_type, SOCK_DGRAM, IPPROTO_UDP); // Create the socket
    if (socket_desc == -1) {
        printf("Could not create socket, %s \n", strerror(errno));
        exit(1);
    }

    /* Set up the port number to be used for connection */
    int port_number = (int)strtol(argv[2], nullptr, 10);
    if(port_number < 1 || port_number > 65536) {
        printf("Port number %s is not valid \n", argv[2]);
        close(socket_desc);
        exit(1);
    }

    ipv4.sin_family = socket_type;
    int err_code = 0;
    if (socket_type == AF_INET) {
        ipv4.sin_port = htons(port_number); /* Convert from host byte order to network byte order */
        /* Set up the IP address to use for our server */
        err_code = inet_pton(AF_INET, argv[1], &(ipv4.sin_addr)); /* Local addr */
    } else if (socket_type == AF_INET6) {
        ipv6.sin6_port = htons(port_number);
        err_code = inet_pton(AF_INET6, argv[1], &(ipv6.sin6_addr)); /* Local addr */
    }

    if( err_code != 1) {
        printf("Invalid IP address: %s \n", argv[1]);
        close(socket_desc);
        exit(1);
    }

    if(socket_type == AF_INET) {
        /* Bind the socket to the specified network address */
        if (bind(socket_desc, (struct sockaddr *) &ipv4, sizeof(ipv4)) < 0) {
            printf("Failed to bind socket to provided IPV4 address, %s \n", strerror(errno));
            exit(1);
        }
    } else {
        if (bind(socket_desc, (struct sockaddr *) &ipv6, sizeof(ipv6)) < 0) {
            printf("Failed to bind socket to provided IPV6 address, %s \n", strerror(errno));
            exit(1);
        }
    }

    //listen(socket_desc, (int)(max_clients + 5));   /* Starts listening for new requests */
    printf("Server started at port number %d \n", port_number);

    return socket_desc;
}


FILE* create_temp_file(FILE *new_file, char* temp_file_name) {
    unsigned seed = time(nullptr);
    std::mt19937 rand_num(seed);
    uint16_t rand_integer = rand_num();

    sprintf(temp_file_name, "newFile_%d.txt", rand_integer);
    FILE* temp_fd = fopen(temp_file_name, "w");
    int currChar, nextChar = -1;
    while(true) {
        currChar = getc(new_file);
        if(nextChar >= 0) {
            fputc(nextChar, temp_fd);
        }
        if(currChar == EOF) {
            if(ferror(temp_fd)) {
                printf("Could not open file, %s \n", strerror(errno));
                exit(1);
            }
            break;
        } else if(currChar == '\n') {
            currChar = '\r';
            nextChar = '\n';
        } else if(currChar == '\r') {
            nextChar = '\0';
        } else{
            nextChar = -1;
        }
        fputc(currChar, temp_fd);
    }
    fclose(temp_fd);
    return fopen(temp_file_name, "r");
}

ssize_t send_data(int write_fd, uint16_t type, uint16_t uiBlockNumber, uint8_t data[], size_t message_length, struct sockaddr_in * address, socklen_t* socket_len){
    tftp_msg data_message{};
    data_message.msgDefault.uiOpcode = htons(type);

    if(type == OPCODE_ACK) {
        message_length = 4;
        data_message.ackMessage.uiBlockNumber = htons(uiBlockNumber);
    }
    else if(type == OPCODE_DATA) {
        data_message.dataMessage.uiBlockNumber = htons(uiBlockNumber);
        memcpy(data_message.dataMessage.data, data, message_length);
        message_length += 4;
    }
    else if(type == OPCODE_ERR) {
        data_message.errorMessage.uiErrorCode = htons(uiBlockNumber);
        memcpy(data_message.errorMessage.uiErrorData, data, message_length);
        message_length += 4;
    }
    else if(type == OPCODE_RRQ || type == OPCODE_WRQ) {
        memcpy(data_message.requestMessage.uiFileNameMode, data, message_length);
        message_length += 4;
    }
    else {
        printf("Bad opcode in send_data func \n");
        exit(1);
    }
    return sendto(write_fd, &data_message, message_length, 0, (struct sockaddr *) address, *socket_len);
}

ssize_t receive_data(int client_socket, tftp_msg* msg, struct sockaddr_in* client_address, socklen_t* client_length){
    ssize_t received_size = recvfrom(client_socket, msg, sizeof(*msg), 0, (struct sockaddr *) client_address, client_length);
    if(errno != EAGAIN && received_size < 0) {
        printf("Error while receiving, length too low, %s \n", strerror(errno));
    } else if(received_size == 0) {
        printf("Zero size msg received, %s \n", strerror(errno));
    }
    return received_size;
}

void exit_child(const char* msg, int socket, sockaddr_in* client_socket, socklen_t* socket_length);
void exit_child(const char* msg, int socket, sockaddr_in* client_socket, socklen_t* socket_length) {
    printf("\n Unexpected error occurred\n %s from:%s:%d \n", msg, inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
    char MSG_SEND[512];
    strcpy(MSG_SEND, msg);
    if(socket > 0)
        send_data(socket, OPCODE_ERR, 0, (uint8_t *)MSG_SEND, strlen(MSG_SEND), client_socket, socket_length);
    close(socket);
    exit(-1);
}


void process_request(tftp_msg message, ssize_t length_of_msg, sockaddr_in* client_socket, socklen_t* socket_length)
{
    tftp_msg data_received{};
    int new_client_sock, mode_of_transfer, num_retries, flag = 0;
    uint8_t data[FILE_SIZE];
    unsigned long sent_data_length, received_data_length;
    uint16_t num_block = 0;
    uint64_t total_file_size = 0;
    timeval timeout_value = {1, 0};                        // 4 seconds of timeout
    char *file_name, *mode_checker, *end;
    char temp_msg[FILE_SIZE] = {0};
    char name_of_file[50];
    FILE *file_1, *file_2;

    uint16_t received_opcode = ntohs(message.msgDefault.uiOpcode);

    // New socket number for handling data
    if ((new_client_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        printf("Error creating new child socket, quitting now, %s \n", strerror(errno));
        exit(-1);
    } else if (setsockopt(new_client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout_value, sizeof(timeout_value)) < 0) {
        printf("Could not set socket options, exiting now. %s \n", strerror(errno));
        exit(-1);
    }

    file_name = (char *)message.requestMessage.uiFileNameMode;
    end = &file_name[length_of_msg - 3];

    if (*end != '\0') {
        exit_child("Invalid filename or mode requested", new_client_sock, client_socket, socket_length);
    }

    mode_checker = strchr(file_name, '\0') + 1;                 // First occurrence of null
    if (mode_checker > end) {
        exit_child("This transfer mode is not valid, try again", new_client_sock, client_socket, socket_length);
    }

    file_1 = fopen(file_name, received_opcode == OPCODE_RRQ ? "r" : "w");
    if (file_1 == nullptr) {
        sprintf(temp_msg, "Requested file %s has not been found, try again", file_name);
        exit_child(temp_msg, new_client_sock, client_socket, socket_length);
    }

    if (strcasecmp(mode_checker, "NETASCII") == 0) {
        mode_of_transfer = ASCII_MODE;
    } else if (strcasecmp(mode_checker, "OCTET") == 0) {
        mode_of_transfer = OCTET_MODE;
    } else {
        sprintf(temp_msg, "This transfer mode(%s) is not valid, try again", mode_checker);
        exit_child(temp_msg, new_client_sock, client_socket, socket_length);
    }
    printf("New file %s request from %s:%u for file %s in %s mode \n", ntohs(message.msgDefault.uiOpcode) == OPCODE_RRQ ? "read" : "written", inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port),
           file_name, mode_checker);

    if (received_opcode == OPCODE_RRQ)
    {
        if (mode_of_transfer == ASCII_MODE) {
            printf("Changing to ASCII format \n");
            file_2 = create_temp_file(file_1, name_of_file);
            fclose (file_1);
            file_1 = file_2;
        }
        while (!flag) {
            sent_data_length = fread(data, 1, sizeof(data), file_1);
            num_block++;
            total_file_size++;

            if (sent_data_length < 512) {
                flag = 1;		// terminating data block
            }
            num_retries = NUM_OF_RETRIES;
            do {
                if (send_data(new_client_sock, OPCODE_DATA, num_block, data, sent_data_length, client_socket, socket_length) < 0) {
                    printf("Error in communication, closing socket %s:%u!\n", inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
                    close(new_client_sock);
                    exit(-1);
                } else {
                    printf("Sent block %u, ", num_block);
                    received_data_length = receive_data(new_client_sock, &data_received, client_socket, socket_length);
                    if (received_data_length >= 0 && received_data_length < 4) {
                        sprintf(temp_msg, "Required size is invalid, try again");
                        exit_child(temp_msg, new_client_sock, client_socket, socket_length);
                    }else if (ntohs(data_received.ackMessage.uiBlockNumber) < num_block) {
                        num_retries--;
                        printf("retrying \n");
                        continue;
                    } else {
                        printf(" received ACK = %u, total = %lu \n", num_block, total_file_size);
                        break;
                    }
                }
                num_retries--;
            } while(num_retries > 0);

            if (num_retries == 0) {
                printf("Transfer session has failed, reached max retries \n");
                exit(-1);
            }
            if (ntohs(data_received.msgDefault.uiOpcode) == OPCODE_ERR) {
                printf("Error message %u:%s received from %s:%u\n", ntohs(data_received.errorMessage.uiErrorCode),
                       data_received.errorMessage.uiErrorData, inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
                exit(-1);
            }
            if (ntohs(data_received.msgDefault.uiOpcode) != OPCODE_ACK) {
                sprintf(temp_msg, "Expected ACK but received %u, try again", ntohs(data_received.msgDefault.uiOpcode));
                exit_child(temp_msg, new_client_sock, client_socket, socket_length);
            }
        }
        printf("\n Sent %lu blocks to %s:%u, for file %s \n ", total_file_size, inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port), file_name);
    }
    else if (received_opcode == OPCODE_WRQ) {
        flag = 0;
        num_block = 1;
        printf("Write request received from %s:%u, for file %s\n", inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port), file_name);
        if (send_data(new_client_sock, OPCODE_ACK, 0, (uint8_t *)temp_msg, 0, client_socket, socket_length) < 0) {
            printf("Session terminated for %s:%u!\n", inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
            exit(-1);
        }
        while (!flag) {
            num_retries = NUM_OF_RETRIES;
            do {
                received_data_length = receive_data(new_client_sock, &data_received, client_socket, socket_length);
                if (received_data_length >= 0 && received_data_length < 4) {
                    strcpy(temp_msg, "Invalid req size received ");
                    exit_child(temp_msg, new_client_sock, client_socket, socket_length);
                }
                if (ntohs(data_received.ackMessage.uiBlockNumber) < num_block) {
                    num_retries--;
                    send_data(new_client_sock, OPCODE_ACK, ntohs(data_received.ackMessage.uiBlockNumber), (uint8_t *)temp_msg, strlen(temp_msg), client_socket, socket_length);
                    continue;
                }
                if (received_data_length >= 4 && send_data(new_client_sock, OPCODE_ACK, num_block, (uint8_t *)temp_msg, strlen(temp_msg), client_socket, socket_length) >= 0) {
                    break;
                }
                if (errno != EAGAIN) {
                    printf("Session terminated for %s:%u!\n", inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
                    exit(-1);
                }
                num_retries--;
            }while(num_retries > 0);

            if (num_retries == 0) {
                printf("No response from %s:%u!\n", inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
                exit(-1);
            }
            num_block++;
            total_file_size++;
            printf("Received block %u, ", num_block);
            if (received_data_length < sizeof(data_received.dataMessage)) {
                flag = 1;
            }
            if (ntohs(data_received.msgDefault.uiOpcode) != OPCODE_DATA) {
                sprintf(temp_msg, "Unexpected behaviour(should have been OPCODE_DATA), try again");
                exit_child(temp_msg, new_client_sock, client_socket, socket_length);
            }
            if (ntohs(data_received.msgDefault.uiOpcode) == OPCODE_ERR) {
                printf("Error message %u:%s received for %s:%u\n", ntohs(data_received.errorMessage.uiErrorCode),
                       data_received.errorMessage.uiErrorData, inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
                exit(-1);
            }
            received_data_length = fwrite(data_received.dataMessage.data, 1, received_data_length - 4, file_1);
            if (received_data_length < 0) {
                printf("Error in data write to file, %s \n", strerror(errno));
                exit(-1);
            }
            if (send_data(new_client_sock, OPCODE_ACK, num_block, (uint8_t *)temp_msg, strlen(temp_msg), client_socket, socket_length) < 0) {
                printf("Session terminated for %s:%u!\n", inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
                exit(-1);
            } else {
                printf(" sent ACK %u, total = %lu \n", num_block, total_file_size);
            }
        }
        printf("\n Written %lu blocks from %s:%u, for file %s \n ", total_file_size, inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port), file_name);
    }

    printf("Data transfer done successfully for %s:%u!\n", inet_ntoa(client_socket->sin_addr), ntohs(client_socket->sin_port));
    fclose(file_1);
    remove(name_of_file);
    close(new_client_sock);
    exit(0);
}
