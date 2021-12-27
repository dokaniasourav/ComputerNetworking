
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
#include <ctime>

extern int errno;
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


#define ADD_CLIENT 111
#define REM_CLIENT 222
#define UPD_CLIENT 333
#define GET_CLIENT 444
#define GET_CL_WITH_UNAME 555


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

struct connected_clients {
    int id = -1;
    char uname[100] = "";
    long last_t = -1;
};

int setup_server(int argc, char **argv);
int sendData(int socket_id, sbcp_msg data_to_send);
sbcp_msg getData(int socket_id);
sbcp_msg sbcp_set_msg(int type_of_conn, char username[], char msg_data[]);
int change_client_ds(int operation, connected_clients client[], int clientID, char username[]);

int num_conn_clients = 0;

int main(int argc, char *argv[])
{
    sockaddr_in client_address{};
    memset((char *)&client_address, 0, sizeof(client_address));
    socklen_t client_length = sizeof(client_address);
    connected_clients clients[100];
    int server_socket = setup_server(argc, argv);
    char msg_data_1[BUFF_SIZE], msg_data_2[BUFF_SIZE];;

    fd_set all_sockets, ready_sockets;
    FD_ZERO(&all_sockets);
    FD_SET(server_socket, &all_sockets);

    long max_clients = strtol(argv[3], nullptr, 10);   // maximum number of clients
    if(max_clients > 1024 || max_clients < 1) {
        printf("Invalid number of clients entered, %s \n", argv[3]);
        exit(1);
    }

    /* Get ready for listening to n number of clients */
    int max_sd;
    max_sd = server_socket;

    uint64_t counter = 0;
    while (true) {
        ready_sockets = all_sockets;
        timeval wait_time = {0, 100000};
        int select_ret = select(max_sd+1, &ready_sockets, nullptr, nullptr, &wait_time);

        if ((select_ret < 0) && (errno != EINTR)) {
            printf("Error selecting sockets, %s \n", strerror(errno));
            exit(1);
        }

        for(int sock_i=0; sock_i <= max_sd; sock_i++) {
            if(FD_ISSET(sock_i, &ready_sockets)) {
                //printf("Change detected in %d\n", sock_i);
                if(sock_i == server_socket ) {
                    int new_client_sock = accept(server_socket, (struct sockaddr *) &client_address, &client_length);
                    if (new_client_sock < 0) {
                        printf("Internal Error: %s,\t could not accept new connection \n", strerror(errno));
                        exit(1);
                    }
                    FD_SET(new_client_sock, &all_sockets);
                    printf("New connection from sd = %d, ip: %s:%d \n",
                           new_client_sock , inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                    if(new_client_sock > max_sd)
                        max_sd = new_client_sock;
                } else {
                    sbcp_msg recvd_data = getData(sock_i);
                    if(recvd_data.head.length == 0) {
                        int client_id = change_client_ds(GET_CLIENT, clients, sock_i, nullptr);
                        sbcp_msg data_to_send = sbcp_set_msg(HEAD_TYPE_OFFLINE, clients[client_id].uname, nullptr);
                        num_conn_clients--;
                        printf("Client connection ended with %s \n",
                            clients[client_id].uname);
                        change_client_ds(REM_CLIENT, clients, sock_i, nullptr);
                        FD_CLR(sock_i, &all_sockets);
                        if(num_conn_clients == 0)
                            printf(" \n\t Char server is empty right now... \n");
                        for (int client_j = 0; client_j < num_conn_clients; ++client_j) {
                            sendData(clients[client_j].id, data_to_send);
                        }

                        continue;
                    }
                    if(recvd_data.head.type == HEAD_TYPE_JOIN) {
                        if(num_conn_clients >= max_clients) {
                            printf("The max number of connections have been reached.. \n");
                            sprintf(msg_data_1,
                                    "Sorry %s, maximum number of clients(%d) are already connected, join another time",
                                    recvd_data.attr[0].payload, num_conn_clients);
                            sendData(sock_i, sbcp_set_msg(HEAD_TYPE_NAK, recvd_data.attr[0].payload, msg_data_1));
                            FD_CLR(sock_i, &all_sockets);
                        } else if( change_client_ds(ADD_CLIENT, clients, sock_i, recvd_data.attr[0].payload) == -1) {
                            sprintf(msg_data_1, "The username %s is already taken, choose another", recvd_data.attr[0].payload);
                            sendData(sock_i, sbcp_set_msg(HEAD_TYPE_NAK, recvd_data.attr[0].payload, msg_data_1));
                            FD_CLR(sock_i, &all_sockets);
                        } else {
                            num_conn_clients++;
                            printf("%s successfully joined the chatroom \n", recvd_data.attr[0].payload);
                            bzero(msg_data_1, sizeof(msg_data_1));
                            if(num_conn_clients > 1) {
                                strcpy(msg_data_1, "\n\t List of connected users: \n\t");
                                for (int i = 0; i < num_conn_clients; i++) {
                                    if (clients[i].id != sock_i)
                                        sprintf(msg_data_1, "%s %d --> %s \n\t", msg_data_1, i, clients[i].uname);
                                }
                            } else {
                                sprintf(msg_data_1, "Welcome, you are the first client to connect ..");
                            }
                            fflush(STDIN_FILENO);
                            sbcp_msg to_send = sbcp_set_msg(HEAD_TYPE_ACK, recvd_data.attr[0].payload, msg_data_1);
                            sendData(sock_i, to_send);

                            sbcp_msg data_to_send = sbcp_set_msg(HEAD_TYPE_ONLINE, recvd_data.attr[0].payload, nullptr);
                            for (int client_j = 0; client_j < num_conn_clients; ++client_j) {
                                if(clients[client_j].id != sock_i) {
                                    sendData(clients[client_j].id, data_to_send);
                                }
                            }
                        }
                        continue;
                    }
                    /* If the head type is not JOIN */
                    if(recvd_data.head.type == HEAD_TYPE_SEND) {
                        if(recvd_data.head.length < 2) {
                            printf("Error in the head msg data \n");
                        } else {
                            printf("Received text from %s : %s \n", recvd_data.attr[0].payload, recvd_data.attr[1].payload);
                            sbcp_msg data_to_send = sbcp_set_msg(HEAD_TYPE_FWD, recvd_data.attr[0].payload, recvd_data.attr[1].payload);
                            for (int client_j = 0; client_j < num_conn_clients; ++client_j) {
                                if(clients[client_j].id != sock_i) {
                                    sendData(clients[client_j].id, data_to_send);
                                } else {
                                    clients[client_j].last_t = time(nullptr);
                                };
                            }
                        }
                    }
                }
            }
        }

        /* Timeout case */
        for(int i=0; i<num_conn_clients; i++) {
            if(std::time(nullptr) - clients[i].last_t >= 10) {      /* The 10-second timer for all clients */
                sbcp_msg data_to_send = sbcp_set_msg(HEAD_TYPE_IDLE, clients[i].uname, nullptr);
                sendData(clients[i].id, data_to_send);
                clients[i].last_t = std::time(nullptr);
            }
        }
    }
    return 0;
}

int setup_server(int argc, char **argv) {

    if (argc != 4) {
        printf("Enter the IP_Address, Port_number and Max_clients as arguments \n");
        exit(1);
    }

    long max_clients = strtol(argv[3], nullptr, 10);   // maximum number of clients
    if(max_clients > 1024 || max_clients < 1) {
        printf("Invalid number of clients entered, %s \n", argv[3]);
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

    int socket_desc = socket(socket_type, SOCK_STREAM, 0); // Create the socket
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

    /* IPV4 vs IPV6 setting */
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

    if(socket_type == AF_INET){
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

    listen(socket_desc, (int)(max_clients + 5));   /* Starts listening for new requests */
    printf("Started Listing to port %d \n", port_number);

    return socket_desc;
}

int change_client_ds(int operation, connected_clients client[], int clientID, char username[]) {
    int last_ele = 0;
    while (client[last_ele].id != -1 && last_ele < 100) {
        last_ele++;
    }
    if(last_ele == 100) {
        printf("Error in storage, too many clients \n");
        exit(1);
    }

    if(operation == ADD_CLIENT || operation == GET_CL_WITH_UNAME) {
        if(strlen(username) <= 0) {
            printf("Empty username not allowed \n");
            return -1;
        }
        for(int i=0; i<last_ele; i++) {
            if(strcmp(client[i].uname, username) == 0) {
                if(operation == GET_CL_WITH_UNAME) {
                    return i;
                } else {
                    printf("Username %s already exists with ClientID = %d \n", username, client[i].id);
                    return -1;
                }
            }
        }
        strcpy(client[last_ele].uname, username);
        client[last_ele].id = clientID;
        client[last_ele].last_t = std::time(nullptr);
        //printf("Added ID: %d and timer=%ld \n", clientID, client[last_ele].last_t);
    }
    else if(operation == REM_CLIENT || operation == GET_CLIENT) {
        int found_ele = 0;
        while ((client[found_ele].id != clientID) && (found_ele <= last_ele)) {
            found_ele++;
        }
        if(found_ele > last_ele) {
            printf("Error removing the client, ID = %d not found \n", clientID);
            exit(1);
        }
        if(operation == GET_CLIENT) {
            return found_ele;
        } else {
            client[found_ele] = client[last_ele-1];
            client[last_ele-1].id = -1;
        }
        last_ele = found_ele;
    }
    return last_ele;
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

        for (int k = 0; k < msgData[index]; k++) {
            new_data.attr[j].payload[k] = (char) msgData[index];
            index += 1;
        }
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
        printf("Error sending the msg data, %s \n", strerror(errno));
        exit(1);
    }

    return 0;
}

sbcp_msg sbcp_set_msg(int type_of_conn, char username[], char msg_data[]) {
    sbcp_msg msg_1;
    msg_1.head.version = 3;
    msg_1.head.type = type_of_conn;
    for(auto & attr_item : msg_1.attr) {
        bzero(attr_item.payload, SIZE_MSG_);
        attr_item.type = attr_item.length = attr_item.client_count = 0;
    }

    msg_1.head.length = 1;
    msg_1.attr[0].type = ATTR_TYPE_UNAME_;
    strcpy(msg_1.attr[0].payload, username);
    msg_1.attr[0].length = strlen(username);
    msg_1.attr[0].client_count = num_conn_clients;

    if (msg_data != nullptr) {
        msg_1.head.length = 2;
        msg_1.attr[1].type = ATTR_TYPE_MSGS_;
        strcpy(msg_1.attr[1].payload, msg_data);
        msg_1.attr[1].length = strlen(msg_data);
    }
    return msg_1;
}