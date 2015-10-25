#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "duckchat.h"

void Error(const char *msg) {
  perror(msg);
  exit(1);
}

void ProcessRequest(void *buffer) {
  struct request current_request;
  memcpy(&current_request, buffer, sizeof(struct request));
  printf("request type: %d\n", current_request.req_type);
  request_t request_type = current_request.req_type;

  switch(request_type){
    case REQ_LOGIN:
      struct request_login login_request;
      memcpy(&login_request, buffer, sizeof(struct request_login));
      printf("Username: %s\n", login_request.req_username);
      break;
  }

}

int main(int argc, char *argv[]) {
  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  int server_socket;
  int receive_len;
  void* buffer[kBufferSize];
  int port;

  if (argc < 2) {
    fprintf(stderr,"server: no port provided\n");
    exit(1);
  }

  port = atoi(argv[1]);

  memset((char *) &server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if ((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    Error("server: can't open socket\n");
  }

  if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    Error("server: bind failed\n");
  }

  while (1) {
    printf("server: waiting on port %d\n", port);
    receive_len = recvfrom(server_socket, buffer, kBufferSize, 0, (struct sockaddr *) &client_addr, &client_addr_len);
    if (receive_len > 0) {
      buffer[receive_len] = 0;
      ProcessRequest(buffer);
    }
  }
}
