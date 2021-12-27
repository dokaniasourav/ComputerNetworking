//
// Created by sd on 10/18/21.
//
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>

#define BUFF_SIZE 580

#define SIZE_UNAME_ 16
#define SIZE_MSG_ 512
#define SIZE_REASON_ 32
#define SIZE_COUNT_ 2

#define ATTR_TYPE_REASON_ 1
#define ATTR_TYPE_UNAME_ 2
#define ATTR_TYPE_COUNT_ 3
#define ATTR_TYPE_MSGS_ 4

#define HEAD_TYPE_JOIN 2
#define HEAD_TYPE_SEND 4
#define HEAD_TYPE_FWD 3

#define HEAD_TYPE_NAK 5
#define HEAD_TYPE_ACK 7
#define HEAD_TYPE_OFFLINE 6
#define HEAD_TYPE_ONLINE 8
#define HEAD_TYPE_IDLE 9

struct msg_head {
    uint16_t version;
    uint16_t type;
    uint16_t length;
};

struct msg_attr_pl{
    char uname[SIZE_UNAME_];
    char msg[SIZE_MSG_];
    char reason[SIZE_REASON_];
};

struct msg_attr {
    uint16_t type;
    uint16_t length;
    char payload[SIZE_MSG_];
    uint16_t client_count;
};

struct sbcp_msg {
    msg_head head{};
    msg_attr attr[3] = {};
    explicit sbcp_msg(){
        head.version = 3;
        head.type = head.length = 0;
        for(auto & attr_item : attr){
            attr_item.type = attr_item.length = 0;
            bzero(attr_item.payload, SIZE_MSG_);
            attr_item.client_count = 0;
        }
    }
};


int setup_server(int argc, char **argv);
int sendData(int socket_id, sbcp_msg data_to_send);
sbcp_msg getData(int socket_id);
sbcp_msg sbcp_set_msg(int type_of_conn, char* username, char* msg_data = nullptr);

int main(int argc, char *argv[])
{
    //address_str server_address{};

    int client_socket = setup_server(argc, argv);
    char msg_data[BUFF_SIZE] = {0}, username[100];

    printf(">> Connected to %s:%s, trying to join chat.. \n", argv[2], argv[3]);
    strcpy(username, argv[1]);

    sbcp_msg dataReceived, dataToSend = sbcp_set_msg(HEAD_TYPE_JOIN, username);
    sendData(client_socket, dataToSend);

    fd_set read_sockets;

    while (true) {
        /* Setup file descriptors to read from console as well as server */
        FD_ZERO(&read_sockets);
        FD_SET(STDIN_FILENO, &read_sockets);
        FD_SET(client_socket, &read_sockets);

        /* Wait until data is available */
        int select_ret = select(client_socket+1, &read_sockets, nullptr, nullptr, nullptr);
        if ((select_ret < 0) && (errno != EINTR)) {
            printf("Error selecting sockets, %s \n", strerror(errno));
            exit(1);
        }

        /* If Console input is detected */
        if(FD_ISSET(STDIN_FILENO,&read_sockets)) {                      /* Some input from console */
            /* If Ctrl+D was received */
            if( fgets(msg_data, BUFF_SIZE, stdin) == nullptr ) {
                printf(">> Disconnecting from the server... \n");
                close(client_socket);
                break;
            } else {
                msg_data[strcspn(msg_data, "\n")] = 0;                  /* Remove the last newline character from string */
                if(strlen(msg_data) > 0) {
                    dataToSend = sbcp_set_msg(HEAD_TYPE_SEND, username, msg_data);
                    sendData(client_socket, dataToSend);
                } else {
                    printf("\33[2K\r");
                }
            }
        } else if(FD_ISSET(client_socket,&read_sockets)) {
            dataReceived = getData(client_socket);
            if(dataReceived.head.length == 0) {
                printf(">> Connection closed by the server, Bye-bye! \n");
                exit(1);
            }
            if(dataReceived.head.type == HEAD_TYPE_FWD) {
                printf("\33[2K\r");
                printf("> %s: %s\n", dataReceived.attr[0].payload, dataReceived.attr[1].payload);
            } else if(dataReceived.head.type == HEAD_TYPE_ACK) {
                printf("\33[2K\r");
                printf(">> Connected to chat room :) %s \n", dataReceived.attr[1].payload);
            } else if(dataReceived.head.type == HEAD_TYPE_NAK) {
                printf("\33[2K\r");
                printf(">> Server: %s \n", dataReceived.attr[1].payload);
                printf(">> Exiting ... \n");
                close(client_socket);
                break;
            } else if(dataReceived.head.type == HEAD_TYPE_OFFLINE) {
                printf("\33[2K\r");
                printf(">> User %s has left the chat room (OFFLINE); Bye bye!!\n", dataReceived.attr[0].payload);
            } else if(dataReceived.head.type == HEAD_TYPE_ONLINE) {
                printf("\33[2K\r");
                printf(">> User %s has entered the chat room (ONLINE); Welcome!!\n", dataReceived.attr[0].payload);
            } else if(dataReceived.head.type == HEAD_TYPE_IDLE) {
                printf("\33[2K\r");
                printf(">> User %s has been IDLE for more than 10 seconds (IDLE) \n", dataReceived.attr[0].payload);
            }
        }
    }
    printf("\n Disconnected !! \n");
    return 0;
}
int num_conn_clients = -1;

int setup_server(int argc, char **argv) {

    if (argc != 4) {
        printf("Needs username, server_IP and port_num, ex: client420 127.0.0.1 1234\n");
        exit(1);
    }
    sockaddr_in ipv4{};
    sockaddr_in6 ipv6{};

    addrinfo type_of_addr{}, *temp = nullptr;
    type_of_addr.ai_family = PF_UNSPEC;
    type_of_addr.ai_flags = AI_NUMERICHOST;

    // Figure out whether the address is IPv4 or IPv6 and the call the APIs accordingly
    if (getaddrinfo(argv[2], nullptr, &type_of_addr, &temp)) {
        printf("Invalid address entered %s", argv[2]);
        exit(1);
    }

    int socket_type = -1;
    if(temp != nullptr)
        socket_type = temp->ai_family;

    int socket_desc = socket(socket_type, SOCK_STREAM, 0); // Create the socket
    if (socket_desc == -1) {
        printf("Could not create socket, %s \n", strerror(errno));
        exit(1);
    }

    /* Set up the port number to be used for connection */
    int port_number = (int)strtol(argv[3], nullptr, 10);
    if(port_number < 1 || port_number > 65536) {
        printf("Port number %s is not valid \n", argv[3]);
        close(socket_desc);
        exit(1);
    }

    /* IPV4 vs IPV6 setting */
    ipv4.sin_family = socket_type;
    int err_code = 0;
    /* Set up the IP address to use for our server */
    if (socket_type == AF_INET) {
        ipv4.sin_port = htons(port_number); /* Convert from host byte order to network byte order */
        err_code = inet_pton(AF_INET, argv[2], &(ipv4.sin_addr)); /* Local addr */
    } else if (socket_type == AF_INET6) {
        ipv6.sin6_port = htons(port_number);
        err_code = inet_pton(AF_INET6, argv[2], &(ipv6.sin6_addr)); /* Local addr */
    }

    if( err_code != 1) {
        printf("Invalid IP address: %s \n", argv[2]);
        close(socket_desc);
        exit(1);
    }

    if(socket_type == AF_INET){
        if (connect(socket_desc, (struct sockaddr *) &ipv4, sizeof(ipv4)) != 0) {
            printf("Error connecting, %s \n", strerror(errno));
            exit(1);
        }
    } else {
        if (connect(socket_desc, (struct sockaddr *) &ipv6, sizeof(ipv6)) != 0) {
            printf("Error connecting, %s \n", strerror(errno));
            exit(1);
        }
    }
    printf("Client socket successfully created \n");
    return socket_desc;
}

sbcp_msg getData(int socket_id) {
    uint8_t msgData[BUFF_SIZE];
    int16_t index = 0;
    long num_of_bytes = recv(socket_id, msgData, BUFF_SIZE, 0);
    sbcp_msg new_data;

    if(num_of_bytes <= 0) {
        new_data.head.length = 0;
        return new_data;
    }

    new_data.head.version = msgData[index]; index += 1;
    new_data.head.type = msgData[index]; index += 1;
    new_data.head.length = (msgData[index]<<8) + msgData[index+1]; index += 2;
    // reason, uname, client_count, msg
    int array_sizes[] = {0, SIZE_REASON_, SIZE_UNAME_, SIZE_COUNT_, SIZE_MSG_};

    for(int j=0; j<new_data.head.length; j++) {
        if(index >= num_of_bytes) {
            printf("Data overflow \n");
            exit(1);
        }
        new_data.attr[j].type = (msgData[index]<<8) + msgData[index+1];  index += 2;
        new_data.attr[j].length = (msgData[index]<<8) + msgData[index+1];  index += 2;
        if( new_data.attr[j].type > 4 ) {
            printf("Invalid attribute type detected \n");
            exit(1);
        }
        if(j == 0) {
            new_data.attr[j].client_count = (msgData[index] << 8) + msgData[index + 1];
            index += 2;
        }
//        if (new_data.attr[j].length > array_sizes[new_data.attr[j].type] || new_data.attr[j].length < 1) {
//            printf("Invalid message length detected \n");
//            exit(1);
//        }
        for (int k = 0; k < new_data.attr[j].length; k++) {
            new_data.attr[j].payload[k] = (char) msgData[index];
            index += 1;
        }
        //printf("Payload %d = %s, len = %d \n", j, new_data.attr[j].payload, new_data.attr[j].length);
        index++;
    }
    return new_data;
}

int sendData(int socket_id, sbcp_msg data_to_send) {

    uint8_t msgData[BUFF_SIZE];
    int16_t index = 0;

    msgData[index] = data_to_send.head.version; index++;
    msgData[index] = data_to_send.head.type; index++;
    msgData[index] = data_to_send.head.length>>8; index++;
    msgData[index] = data_to_send.head.length&0xFF; index++;

    int array_sizes[] = {0, SIZE_REASON_, SIZE_UNAME_, SIZE_COUNT_, SIZE_MSG_};

    for(int j=0; j<data_to_send.head.length; j++) {
        if(index >= BUFF_SIZE){
            printf("Data overflow \n");
            exit(1);
        }
        if( data_to_send.attr[j].type > 4 ) {
            printf("Invalid attribute type detected \n");
            exit(1);
        }
        msgData[index] = data_to_send.attr[j].type>>8; index++;
        msgData[index] = data_to_send.attr[j].type&0xFF; index++;
        msgData[index] = data_to_send.attr[j].length>>8; index++;
        msgData[index] = data_to_send.attr[j].length&0xFF; index++;

        if(j == 0) {
            msgData[index] = data_to_send.attr[j].client_count >> 8; index++;
            msgData[index] = data_to_send.attr[j].client_count & 0xFF; index++;
        }

        if (data_to_send.attr[j].length > array_sizes[data_to_send.attr[j].type] || data_to_send.attr[j].length < 1) {
            printf("Invalid message length detected \n");
            exit(1);
        }
        for (int k = 0; k < data_to_send.attr[j].length; k++) {
            msgData[index] = data_to_send.attr[j].payload[k];
            index += 1;
        }
        msgData[index] = 0;
        index += 1;
    }

    if(send(socket_id, msgData, index, 0) == -1 ) {
        printf("Error sending the msg data \n");
        exit(1);
    }
    return 0;
}

sbcp_msg sbcp_set_msg(int type_of_conn, char* username, char* msg_data) {
    sbcp_msg msg_1;

    msg_1.head.version = 3;
    msg_1.head.type = type_of_conn;
    for(auto & attr_item : msg_1.attr) {
        bzero(attr_item.payload, SIZE_MSG_);
        attr_item.type = attr_item.length = attr_item.client_count = 0;
    }

    if (type_of_conn == HEAD_TYPE_JOIN || type_of_conn == HEAD_TYPE_SEND) {
        msg_1.head.length = 1;
        msg_1.attr[0].type = ATTR_TYPE_UNAME_;
        strcpy(msg_1.attr[0].payload, username);
        msg_1.attr[0].length = strlen(username);
        msg_1.attr[0].client_count = 0;
    }

    if (type_of_conn == HEAD_TYPE_SEND && msg_data != nullptr) {
        msg_1.head.length = 2;
        msg_1.attr[1].type = ATTR_TYPE_MSGS_;
        strcpy(msg_1.attr[1].payload, msg_data);
        msg_1.attr[1].length = strlen(msg_data);
    }
    return msg_1;
}