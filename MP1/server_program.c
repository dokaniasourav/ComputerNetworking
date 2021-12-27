
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>

extern int errno;
#define BUFF_SIZE 128

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Enter a port number to use, ex: 5625 \n");
        exit(1);
    }
    int socket_desc = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (socket_desc == -1) {
        printf("Could not create socket, %s \n", strerror(errno));
        exit(1);
    }
    printf("Socket successfully created \n");

    /* Setting up local variables to use later in the program */
    char msg_data[BUFF_SIZE];
    char *ptrEnd;
    memset(msg_data, 0, sizeof(msg_data));
    struct sockaddr_in server_address, client_address;
    memset((char *)&server_address, 0, sizeof(server_address));
    memset((char *)&client_address, 0, sizeof(client_address));
    socklen_t client_length = sizeof(client_address);

    /* Set up the port number to be used for connection */
    int port_number = (int)strtol(argv[1], &ptrEnd, 10);
    if(!(port_number > 1024 && port_number < 65535)) {
        printf("Port number %s is not valid \n", argv[1]);
        exit(1);
    }
    server_address.sin_port = htons(port_number); /* Convert from host byte order to network byte order */

    /* Set up the IP address to use for our server */
    int err_code;
    server_address.sin_family = AF_INET;                                        /* using Internet protocol */
    err_code = inet_pton(AF_INET, "127.0.0.1", &(server_address.sin_addr)); /* Local loopback address */
    if( err_code != 1) {
        printf("Error: %s \n", strerror(errno));
        exit(1);
    }

    /* Bind the socket to the specified network address */
    int bind_flag = bind(socket_desc, (struct sockaddr *) &server_address, sizeof(server_address)); /* assigning name to our socket */
    if (bind_flag < 0) {
        printf("Failed to bind socket to provided address, %s \n", strerror(errno));
        exit(1);
    }

    /* Get ready for listening to n number of clients */
    int max_num_clients = 10;
    pid_t client_pid;
    listen(socket_desc, max_num_clients);   /* Starts listening for new requests */
    printf("Started Listing to port %d \n", port_number);

    while(1) {
        /* Accepting new connection requests from clients */
        int listen_socket_new = accept(socket_desc, (struct sockaddr *) &client_address, &client_length);
        if (listen_socket_new < 0) {
            printf("Error accepting connection from client, %s \n", strerror(errno));
            exit(1);
        }
        int new_port_num = ntohs(client_address.sin_port);
        char *new_address = inet_ntoa(client_address.sin_addr);
        printf("New connection est with %s:%d \n", new_address, new_port_num);

        client_pid = fork();                        // Create the new child process which handles messages

        if(client_pid == -1) {                     // In case there is an error in creating new process
            printf("Error creating new process \n");
            close(listen_socket_new);
            continue;
        } else if(client_pid == 0) {                // The Child process to handle messages
            while(1) {
                long num_of_bytes = recv(listen_socket_new, msg_data, BUFF_SIZE, 0);
                if (num_of_bytes == -1) {       /* Error in receiving */
                    printf("Error receiving data \n");
                } else if(num_of_bytes == 0) {  /* EOF was sent by Child */
                    printf("Disconnected %s:%d \n", new_address, new_port_num);
                    close(listen_socket_new);
                    break;
                } else if(strcmp(msg_data, "terminate") == 0) {
                    printf("Exit command received, client [%s:%d] disconnected \n", new_address, new_port_num);
                    close(listen_socket_new);
                    break;
                }

                printf("\t %s:%d :: %s \n", new_address, new_port_num, msg_data);
                if( send(listen_socket_new, msg_data, strlen(msg_data), 0) == -1 ) {    /* Echo the msg back to client */
                    printf("Error sending msg \n");
                    break;
                }
                memset(msg_data, 0, sizeof(msg_data)); /* reset buffer for next cycle */
            }
            close(listen_socket_new);
            exit(1);
        }
    }
    return 0;
}
