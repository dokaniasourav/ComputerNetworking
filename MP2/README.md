# Machine Prob 2 :: Team07(ECEN602)
*Made by Sourav and Jared*

## General description:
**Simple Broadcast Chat Protocal (SBCP)**

##### This program implements the following features

1. The chat service allows at most 1023 clients to be connected
2. If a client has a duplicate username, it would be rejected by the server
3. The server allows a client to use a username that has been used previously
5. The chat service works for both IPv6 and IPv4 (**Bonus Feature 1**)
6. The server could acknowledge all well-formed JOIN requests with an ACK and send a NAK when the request would cause the server to exceed its client limit; the server could send an ONLINE to other clients upon a successful JOIN and send OFFLINE to its remaining clients when a client gets disconnected (**Bonus Feature 2**)
7. The clients that have not used the chat session for more than 10 seconds will send an IDLE message to the server, and the server in turn send an IDLE message to all other clients (**Bonus Feature 3**)

## Architecture:

Server program, created by Sourav Dokania is used by the **server.cpp** file.
This program will ::

1. Starts by using <./server server_ip server_port max_clients>
2. Waits for a new client to connect to its socket
3. When a connected cliebt sends the JOIN SBCP message, its username is verified and added to the server's pool of clients. Then the server sends the ACK frame to acknowledge the client. It also prohibits the clients from using the same username
4. When any messages from client is received, it is forward to all other clients except the one who send it
5. It also takes care of IDLE time for each client, ONLINE and OFFLINE broadast messages and discard any unknown messages
 

Client code is made by Jared Sun, uses **client.cpp** file. This program will ::

1. Connect by using <./client username server_ip server_port>
2. Initiate a JOIN with the server using the username provided on the command line
3. Display a basic prompt to the operator for displaying received FWD messages and typed message that are sent
4. Use select to handle both sending and receiving of messages
5. Show a message when it has been IDLE for more than 10 seconds

## Usage:

### Start the server using

./server server_ip server_port max_clients

### Start the clients using

./client username server_ip server_port

Note: When using the same computer the IP address of 127.0.0.1 should be used

Thank you for reading.
> Sourav Dokania

> Jared Sun
