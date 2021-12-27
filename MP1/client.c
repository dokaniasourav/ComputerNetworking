
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

extern int errno;
#define BUFF_SIZE 64

int main(int argc, char *argv[])
{
    if (argc <= 2) {
        printf("Needs the server address and port number to work, example: 127.0.0.1 5625 \n");
        exit(1);
    }
    int socket_desc = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (socket_desc == -1) {
        printf("Could not create socket, %s \n", strerror(errno));
        exit(1);
    }
    printf("Socket successfully created \n");

    /* Setting up local variables to use later in the program */
    char msg_data[BUFF_SIZE + 2], overflow_msg[10000];
    char *ptrEnd;
    memset(msg_data, 0, sizeof(msg_data));
    struct sockaddr_in server_address;
    memset((char *)&server_address, 0, sizeof(server_address));

    /* Set up the port number to be used for connection */
    int port_number = (int)strtol(argv[2], &ptrEnd, 10);    /* Integer to string of base 10 */
    if(!(port_number > 1024 && port_number < 65536)) {
        printf("Port number %s is not valid \n", argv[2]);
        exit(1);
    }
    server_address.sin_port = htons(port_number); /* Convert from host byte order to network byte order */

    /* Set up the IP address to use for our server */
    int err_code;
    server_address.sin_family = AF_INET;                                /* using Internet protocol IPV4 */
    err_code = inet_pton(AF_INET, argv[1], &(server_address.sin_addr)); /* Local addr conversion */
    if( err_code != 1) {
        printf("Error: %s \n", strerror(errno));
        exit(1);
    }

    int connection_desc = connect(socket_desc, (struct sockaddr *) &server_address, sizeof(server_address));
    if (connection_desc < 0) {
        printf("Error connecting, %s \n", strerror(errno));
        exit(1);
    }
    printf("Connected to server %s:%s \n", argv[1], argv[2]);
    printf("Send \"terminate\" to end this connection \n");

    while (1) {
        printf("Client :: ");
        do {
            if (fgets(msg_data, BUFF_SIZE + 2, stdin) == NULL) {
                printf("EOF detected, disconnecting from server... \n");
                close(connection_desc);
                exit(1);
            }
            msg_data[strcspn(msg_data, "\n")] = 0;            /* Remove the last newline character from string */
        } while( strlen(msg_data) == 0);

        if( strlen(msg_data) > BUFF_SIZE) {                    /* Too large input msg */
            printf("Warning :: Msg too large, ignoring extra overflow \n");
            fgets(overflow_msg, 10000, stdin);
        }
        msg_data[BUFF_SIZE] = 0;

        long num_write_bytes = send(socket_desc, msg_data, strlen(msg_data), 0);
        if (num_write_bytes < 0) {
            printf("Error sending data \n");
            close(connection_desc);
            exit(1);
        }
        // usleep(500000);
        if(strcmp(msg_data, "terminate") == 0) {
            printf("Connection closed by server \n");
            close(connection_desc);
            exit(1);
        }

        if(recv(socket_desc, msg_data, BUFF_SIZE, 0) >= 0){
            printf("Server :: %s\n", msg_data);
        }else{
            printf("Error receiving data\n");
        }
        msg_data[0] = 0;
        memset(msg_data, 0, sizeof(msg_data));
    }
    printf("\n Disconnected !! \n");
    return 0;
}
