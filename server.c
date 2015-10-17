//
// Created by Keith Hamm on 10/17/15.
//

#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include "server.h"

//int SERVER_HOST_IP_ADDRESS = '127.0.0.1';
int PORT = 5000;
int BUFFER_SIZE = 2048;

int main() {
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLen = sizeof(clientAddress);
    int serverSocket;
    int receiveLen;
    unsigned char buffer[BUFFER_SIZE];

    if ((serverSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("server: can't open socket");
        return 0;
    }

    memset((char *) &serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(PORT);


    if (bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        perror("server: bind failed");
        return 0;
    }

    while (1) {
        printf("server: waiting on port %d\n", PORT);
        receiveLen = recvfrom(serverSocket, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &clientAddress, &clientAddressLen);
        if (receiveLen > 0) {
            buffer[receiveLen] = 0;
            printf("received message: \"%s\"\n", buffer);
        }
    }
}