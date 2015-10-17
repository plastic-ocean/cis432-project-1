//
// Created by Keith Hamm on 10/17/15.
//

#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include "client.h"

int PORT = 5001;
int SERVER_PORT = 5000;

int main() {
    struct sockaddr_in clientAddress;
    struct sockaddr_in serverAddress;

    memset((char *) &clientAddress, 0, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    clientAddress.sin_port = htons(PORT);

    memset((char *) &serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(SERVER_PORT);

    unsigned int addressLen;
    int clientSocket;

    printf("Attempting to create socket.\n");
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("client: can't open socket");
        return 0;
    }

    printf("Attempting to bind.\n");
    if (bind(clientSocket, (struct sockaddr *) &clientAddress, sizeof(clientAddress)) < 0) {
        perror("client: bind failed");
        return 0;
    }

    char *message = "test";

    printf("Attempting to send.\n");
    if (sendto(clientSocket, message, strlen(message), 0, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        perror("client: failed to send message");
        return 0;
    }

    return 0;
}