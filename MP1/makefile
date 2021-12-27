make_socket: client_code server_code

client_code: client.c
	gcc client.c -o client

server_code: server_program.c
	gcc server_program.c -o server

clean:
	rm server client
