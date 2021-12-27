# Machine Prob 3 :: Team07(ECEN602)
*Made by Sourav and Jared*

## General description:
**Trivial File Transfer Protocol (TFTP)**

##### This program implements the following features

1. The server could accept the client's read request by sending data message
2. The server could accept the client's write request by sending acknowledgement (**Bonus Feature**)

## Architecture:

The server program will:

1. Server starts a new UDP socket on the user input port, waiting for connection requests
2. Once a request is received, we can check for the type of OPCODE that was received in the request
3. A new child process is started using fork() which allows it to handle client messages independently 
4. Get the file name and mode of transfer, see if file exists and convert file to ascii if ascii mode is specified
5. When READ request is received:
6. Make sure that each data message has fixed size of data block (512 bytes) and is individually acknowledged
7. For each block sent, if ACK is not received we will re-send the block
8. Create a new temp file to write the data
8. When WRITE request is received:
9. Receive each individual blocks of data, and send corresponding ACKs
10. If any ACKs are missed(duplicate block number), send that ACK again
11. The session will end by the last data block containing less than 512 bytes
12. In either case:
13. Complete error recovery using retransmission after timeout
14. The sender will repeat the message after time out when TFTP message is lost and when there is no expected response
15. Repeat the last acknowledgement after time out when the next data message is not received after acknowledgement

## Usage:

### Make using:
```
make
```
### Start the server using
```
./server server_ip server_port
```

Thank you for reading.
> Sourav Dokania

> Jared Sun
