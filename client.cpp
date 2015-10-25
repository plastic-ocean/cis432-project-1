#include "client.h"
#include "duckchat.h"

// Variables
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
int client_socket;


// Prints an error message and exits the program.
void Error(const char *msg) {
  perror(msg);
  exit(1);
}


// Connects to the server at a the given port.
void Connect(char *server, int port) {
  printf("Connecting to %s\n", server);

  srand((unsigned int) time(NULL));
  int client_port = (rand() % 5000) + 4000;

  memset((char *) &client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  client_addr.sin_port = htons(client_port);

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


// Sends messages to the server.
int SendMessage(void *message, size_t message_size) {
  if (sendto(client_socket, message, message_size, 0, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    Error("client: failed to send message\n");
  }

  return 0;
}

// Sends a message to all users in on the active channel.
int Say(std::string msg) {
  void *message = static_cast<void *>(&msg);

  return SendMessage(message, sizeof(message));
}


// Sends login requests to the server.
int RequestLogin(char *username) {
  struct request_login login;
  memset((char *) &login, 0, sizeof(login));
  login.req_type = REQ_LOGIN;
  strncpy(login.req_username, username, USERNAME_MAX);

  size_t size = sizeof(struct request_login);
  void* message[size];
  memcpy(message, &login, size);

  return SendMessage(message, size);
}


// Sends logout requests to the server.
int RequestLogout() {
  struct request_logout logout;
  memset((char *) &logout, 0, sizeof(logout));
  logout.req_type = REQ_LOGOUT;

  size_t size = sizeof(struct request_logout);
  void* message[size];
  memcpy(message, &logout, size);

  return SendMessage(message, size);
}


// Sends join requests to the server.
int RequestJoin(char *channel) {
  struct request_join join;
  memset((char *) &join, 0, sizeof(join));
  join.req_type = REQ_JOIN;
  strncpy(join.req_channel, channel, CHANNEL_MAX);

  size_t size = sizeof(struct request_join);
  void* message[size];
  memcpy(message, &join, size);

  return SendMessage(message, size);
}


// Splits strings around spaces.
std::vector<std::string> StringSplit(std::string input) {
  std::istringstream iss(input);
  std::vector<std::string> result{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

  return result;
}


// Splits strings around spaces.
std::vector<std::string> SplitString(std::string input, char delimiter) {
  std::vector<std::string> result;
  std::string word = "";

  for (char c : input) {
    if (c != delimiter) {
      word += c;
    } else {
      result.push_back(word);
      word = "";
    }
  }
  result.push_back(word);

  return result;
}


// Processes the input string to decide what type of command it is.
bool ProcessInput(std::string input) {
  std::vector<std::string> inputs = StringSplit(input);
  bool result = true;

  if (inputs[0] == "/exit") {
    RequestLogout();
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
  char *username;
  std::string input;

  if (argc < 4) {
    fprintf(stderr,"usage: client [server name] [port] [username]\n");
    exit(1);
  }

  server = argv[1];
  port = atoi(argv[2]);
  username = argv[3];

  Connect(server, port);

  RequestLogin(username);
  RequestJoin((char *) "Common");

  // TODO handle response from send

  while(1) {
    std::cout << "> ";
    getline(std::cin, input);

    if (input[0] == '/') {
      if (!ProcessInput(input)) {
        break;
      }
    } else {
      // Send chat messages
      Say(input);
    }

  }

  return 0;
}