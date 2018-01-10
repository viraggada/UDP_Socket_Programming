# ECEN 5273: Network Systems: Programming Assignment 1 #

Author: Virag Gada

Date: 09/22/2017

Goal: To develop UDP client and servers where the client issues commands
      and the server sends a suitable response to the client.

File structure:
```
./README.md
./clientf/client.c
./clientf/Makefile
./serverf/server.c
./serverf/Makefile
```

client.c: The program creates a UDP socket and sends commands such as get [file],
          put [file], delete [file], ls and exit to the server.

server.c: The program accepts the command from the client and performs the
          appropriate action.

Commands:
```
get [file]: On the client side we send the name of the file we want to get.
            The server checks if it has the file, if not it sends an "error"
            message or it sends the file size that the client should accept.
            We enable timeouts on both sides to handle lost ACK's or packet
            retransmission.

put [file]: We send the put command to the server and then open the file on
            client side, if the file does not exist we send the "error" message
            or the actual file size to expect. The server handles this accordingly.
            We enable timeouts on both sides to handle lost ACK's or packet
            retransmission.

delete [file]: We send the command with the file name of the file to delete.
            If it does not exist or is in use the server replies with an error.

ls: Client sends this command to check the files available on the server side.

exit: Used when the server is to be closed.
```

If the client sends any unknown command the server replies the client back with
the same command stating that it was not recognized.

Execution:
```
Server side - Go to the server folder.
              Run make.
              Run the server using ./server [port no]

Client side - Go to the client folder.
              Run make.
              Run the file using ./client [ip address of server] [port no]
```
