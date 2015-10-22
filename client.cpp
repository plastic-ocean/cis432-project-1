#include "client.h"
#include "duckchat.h"

//#include <stdio.h>
//#include <sys/socket.h>
//#include <string.h>
//#include <netinet/in.h>
//#include <stdlib.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <netinet/in.h>

//using namespace std;

//enum Commands {EXIT, JOIN, LEAVE, LIST, WHO, SWITCH};
int kClientPort = 5001;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
int client_socket;
std::vector<char *> delete_queue;

void Error(const char *msg) {
  perror(msg);
  exit(1);
}

//char *StringToCharStar(std::string input) {
//  char *tmp = new char[input.length() + 1];
//  strcpy(tmp, input.c_str());
//
//  return tmp;
//}

void Connect(char *server, int port) {
  printf("Connecting to %s\n", server);

  memset((char *) &client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  client_addr.sin_port = htons(kClientPort);

  memset((char *) &server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // TODO figure out how to handle ex: ix.cs.uoregon.edu
  server_addr.sin_port = htons(port);

  if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    Error("client: can't open socket\n");
  }

  if (bind(client_socket, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
    Error("client: bind failed\n");
  }
}

void Send(std::string input) {
  char *message = new char[input.length() + 1];
  strcpy(message, input.c_str());

  if (sendto(client_socket, message, strlen(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    Error("client: failed to send message\n");
  }

  delete message;
}

std::vector<std::string> StringSplit(std::string input) {
  std::istringstream iss(input);
  std::vector<std::string> result{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

  return result;
}

bool ProcessInput(std::string input) {
  std::vector<std::string> inputs = StringSplit(input);
  bool result = true;

  if (inputs[0] == "/exit") {
    Send(inputs[0]);
    result = false;
  } else if (inputs[0] == "/list") {

  } else if (inputs[0] == "/join") {

  } else if (inputs[0] == "/leave") {

  } else if (inputs[0] == "/who") {

  } else if (inputs[0] == "/switch") {

  } else {
    std::cout << "Invalid command" << std::endl;
  }

  return result;
}

int main(int argc, char *argv[]) {
  char *server;
  int port;
  std::string username;
  std::string input;
//  delete_queue = std::vector<char *>();

  if (argc < 4) {
    fprintf(stderr,"usage: client [server name] [port] [username]\n");
    exit(1);
  }

  server = argv[1];
  port = atoi(argv[2]);
  username = argv[3];

  Connect(server, port);

  Send(username);

  // TODO handle response from send

  while(1) {
    std::cout << "> ";
    getline(std::cin, input);

    if (input[0] == '/') {
      if (!ProcessInput(input)) {
        break;
      }
    } else {
      // Sending chat messages
      Send(input);
    }

  }

  return 0;
}
