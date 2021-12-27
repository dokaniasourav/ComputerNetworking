# Machine prob 1 :: Team07(ECEN602)
*Made by Jared and Sourav*

## General description:
***Socket programming for a server with multiple clients***

1. Using the standard socket.h library
2. TCP-IP Connection used with IP4V
3. Server supports multiple clients (max no of clients can be changed by MAX_NUM_CLIENTS define)
4. Connection can be terminated by EOF (Ctrl + D) or by sending "terminate"
5. Sends the entire length of buffer, specified by MAX_BUFF, truncates the rest

## Architecture:

Server program, created by Sourav Dokania is used by the **server_program.c** file.
This program will ::

1. Create a new socket
2. Binds this new socket with port number entered by user (port number should be between 1024 and 65365)
3. Start listening to the *address:port*. We are using 127.0.0.1 as the default IP address
4. When a client sends the connection request, a new child process is created to handles this client. Meanwhile parent process still waits for additional client connection.
5. Inside the child process: the server waits for client to send new data
6. If EOF or "terminate" is received in data, the connection is ended and child process is terminated
7. Otherwise, we print the message and send back the text to client

* If there are any errors during the entire process, the error message is printed along with the reason for error *

Client code is made by Jared Sun, uses **client.c** file. This program ::

1. Creates a new socket
2. Connects to the server using the IP and port received from user (in case of error, appropriate messages are displayed)
3. Waits for the user to enter new data through stdin, if data is more than length accepted then it truncates it to buffer length
4. Sends the data to server, waits for the server's response ...
5. After server's response, it prints the message from server
6. Continues to read stdin, until it receives a EOF or terminate command

## Usage:

### Start the server using

./server PORT_NUMBER

example ::
```
./server 8565
 ```
 ![Server](https://github.tamu.edu/souravdokania/ECEN602_Team07/blob/master/MP1/Server.png)


### Now start the clients using

./client IP_ADDR  PORT_NUMBER

example ::
```
./client 127.0.0.1 8565
 ```
![Client](https://github.tamu.edu/souravdokania/ECEN602_Team07/blob/master/MP1/client.png)

Note: When using the same computer the IP address of 127.0.0.1 should be used


Continue to send data from the client, which will reflect on the server. Server will then echo back to the client...

Thank you for reading.
> Jared Sun 

> Sourav Dokania

