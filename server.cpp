#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <cstring>
#include <stdio.h>
#include <string>
#include <iostream>

#include "server.h"
#include "duckchat.h"


std::string user;


void Error(const char *msg) {
  perror(msg);
  exit(1);
}


void ProcessRequest(void *buffer) {
  struct request current_request;
  memcpy(&current_request, buffer, sizeof(struct request));
  std::cout << "request type: " << current_request.req_type << std::endl;
  request_t request_type = current_request.req_type;

  switch(request_type){
    case REQ_LOGIN:
      struct request_login login_request;
      memcpy(&login_request, buffer, sizeof(struct request_login));
      user = login_request.req_username;
      std::cout << "server: " << login_request.req_username << " logs in" << std::endl;
      break;
    case REQ_LOGOUT:
      struct request_logout logout_request;
      memcpy(&logout_request, buffer, sizeof(struct request_logout));
      std::cout << "server: " << user << " logs out" << std::endl;
      break;
    case REQ_JOIN:
      struct request_join join_request;
      memcpy(&join_request, buffer, sizeof(struct request_join));
      std::cout << "server: " << user << " joins channel " << join_request.req_channel << std::endl;
      break;
    default:
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

  user = "";

  if (argc < 2) {
    std::cerr << "server: no port provided" << std::endl;
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

  printf("server: waiting on port %d\n", port);
  while (1) {
    receive_len = recvfrom(server_socket, buffer, kBufferSize, 0, (struct sockaddr *) &client_addr, &client_addr_len);
    if (receive_len > 0) {
      buffer[receive_len] = 0;
      ProcessRequest(buffer);
    }
  }
}
