#ifndef CIS432_PROJECT_1_SERVER_H
#define CIS432_PROJECT_1_SERVER_H

int kBufferSize = 2048;

class Channel {
public:
  std::string name;
  std::list<User *> users;

  Channel(std::string name): name(name) {};
};

class User {
public:
  std::string name;
  int port;
  struct sockaddr_in *address;
  std::list<Channel *> channels;

  User(std::string name, int port, struct sockaddr_in *address): name(name), port(port), address(address) {};
};

void Error(const char *msg);
void ProcessRequest(void *buffer);

#endif //CIS432_PROJECT_1_SERVER_H
