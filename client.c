#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include "client.h"

int kClientPort = 5001;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
int client_socket;

void Error(const char *msg) {
  perror(msg);
  exit(1);
}

void Connect(char *server, int port) {
  printf("Connecting to %s\n", server);

  memset((char *) &client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  client_addr.sin_port = htons(kClientPort);

  memset((char *) &server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    Error("client: can't open socket\n");
  }

  if (bind(client_socket, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
    Error("client: bind failed\n");
  }
}

void Send(char *message) {
  if (sendto(client_socket, message, strlen(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    Error("client: failed to send message\n");
  }
}

int main(int argc, char *argv[]) {
  char *server;
  int port;
  char *username;

  if (argc < 4) {
    fprintf(stderr,"usage: client [server name] [port] [username]\n");
    exit(1);
  }

  server = argv[1];
  port = atoi(argv[2]);
  username = argv[3];

  Connect(server, port);

  Send(username);

  return 0;
}