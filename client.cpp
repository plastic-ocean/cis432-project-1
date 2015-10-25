#include <netdb.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include "client.h"
#include "duckchat.h"

// Variables
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
int client_socket;
struct addrinfo *server_info;
char *channel;


// Prints an error message and exits the program.
void Error(const char *msg) {
  std::cerr << msg << std::endl;
  exit(1);
}


// Connects to the server at a the given port.
void Connect(char *domain, const char *port) {
  std::cout << "Connecting to " << domain << std::endl;

  struct addrinfo hints;
  struct addrinfo *server_info_tmp;
  int status;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;

  if ((status = getaddrinfo(domain, port, &hints, &server_info_tmp)) != 0) {
    std::cerr << "client: unable to resolve address: " << gai_strerror(status) << std::endl;
    exit(1);
  }

  // getaddrinfo() returns a list of address structures into server_info_tmp.
  // Try each address until we successfully connect().
  // If socket() (or connect()) fails, close the socket and try the next address.

  for (server_info = server_info_tmp; server_info != NULL; server_info = server_info->ai_next) {
    if ((client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol)) < 0) {
      continue;
    }
    if (connect(client_socket, server_info->ai_addr, server_info->ai_addrlen) != -1) {
      fcntl(client_socket, F_SETFL, O_NONBLOCK);
      break; // Success
    }
    close(client_socket);
  }



  if (server_info == NULL) {
    Error("client: all sockets failed to connect");
  }
}


// Sends a message to all users in on the active channel.
int RequestSay(const char *message) {
  struct request_say say;
  memset(&say, 0, sizeof(say));
  say.req_type = REQ_SAY;
  strncpy(say.req_text, message, SAY_MAX);
  strncpy(say.req_channel, channel, CHANNEL_MAX);

  if (sendto(client_socket, &say, sizeof(say), 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    Error("client: failed to send message\n");
  }

  return 0;
}


// Sends login requests to the server.
int RequestLogin(char *username) {
  struct request_login login;
  memset(&login, 0, sizeof(login));
  login.req_type = REQ_LOGIN;
  strncpy(login.req_username, username, USERNAME_MAX);

  size_t message_size = sizeof(struct request_login);

  if (sendto(client_socket, &login, message_size, 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    Error("client: failed to request login\n");
  }

  return 0;
}


// Sends logout requests to the server.
int RequestLogout() {
  struct request_logout logout;
  memset((char *) &logout, 0, sizeof(logout));
  logout.req_type = REQ_LOGOUT;

  size_t message_size = sizeof(struct request_logout);

  if (sendto(client_socket, &logout, message_size, 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    Error("client: failed to request logout\n");
  }

  return 0;
}


// Sends join requests to the server.
int RequestJoin(char *channel) {
  struct request_join join;
  memset((char *) &join, 0, sizeof(join));
  join.req_type = REQ_JOIN;
  strncpy(join.req_channel, channel, CHANNEL_MAX);

  size_t message_size = sizeof(struct request_join);

  if (sendto(client_socket, &join, message_size, 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    Error("client: failed to request join\n");
  }

  return 0;
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
  char *domain;
  char *port_str;
  int port_num;
  char *username;
  std::string input;

  struct timeval timeout;
  fd_set read_set;
  int file_desc = 0;
  int result;
  char receive_buffer[SAY_MAX];
  memset(&receive_buffer, 0, SAY_MAX);

  if (argc < 4) {
    Error("usage: client [server name] [port] [username]");
  }

  domain = argv[1];
  port_str = argv[2];
  port_num = atoi(argv[2]);
  username = argv[3];

  if (strlen(domain) > UNIX_PATH_MAX) {
    Error("client: server name must be less than 108 characters");
  }

  if (port_num < 0 || port_num > 65535) {
    Error("client: port number must be between 0 and 65535");
  }

  if (strlen(username) > USERNAME_MAX) {
    Error("client: username must be less than 32 characters");
  }

  Connect(domain, port_str);

  RequestLogin(username);

  channel = (char *) "Common";
  RequestJoin(channel);

  // TODO handle response from send

  while(1) {
    FD_ZERO(&read_set);
    FD_SET(file_desc, &read_set);

    timeout.tv_sec = 0; // TODO change time value?
    timeout.tv_usec = 0;

//    if ((result = select(file_desc + 1, &read_set, NULL, NULL, NULL)) < 0) {
//      continue;
//    }
    if ((result = select(file_desc + 1, &read_set, NULL, NULL, &timeout)) < 0) {
      Error("client: problem using select");
    }

//    size_t size = sizeof(receive_buffer);

//    std::cout << "past select, result: " << result << std::endl;

    if (result > 0) {
      if (FD_ISSET(file_desc, &read_set)) {
        // Socket has data
        result = read(client_socket, receive_buffer, sizeof(receive_buffer));
      }

      if (result == 0) {
        close(file_desc);
      } else {
        // TODO capture user input, store, clean input, then print buffer, afterward replace input
        std::cout << "[" << channel << "]" << "[" << username << "]: " << receive_buffer << std::endl;
      }
    }

//    std::cout << "past check result" << std::endl;

//    std::cout << "> ";
//    getline(std::cin, input);
//
//    if (input[0] == '/') {
//      if (!ProcessInput(input)) {
//        break;
//      }
//    } else {
//      // Send chat messages
//      RequestSay(input.c_str());
//      std::cout << "[" << channel << "]" << "[" << username << "]: " << input << std::endl;
//    }

    std::cout << "> ";
    getline(std::cin, input);

    if (input[0] == '/') {
      if (!ProcessInput(input)) {
        break;
      }
    } else {
      // Send chat messages
      RequestSay(input.c_str());
      std::cout << "[" << channel << "]" << "[" << username << "]: " << input << std::endl;
    }

//    if (FD_ISSET(STDIN_FILENO, &read_set)) {
//      std::cout << "> ";
//      getline(std::cin, input);
//
//      if (input[0] == '/') {
//        if (!ProcessInput(input)) {
//          break;
//        }
//      } else {
//        // Send chat messages
//        RequestSay(input.c_str());
//        std::cout << "[" << channel << "]" << "[" << username << "]: " << input << std::endl;
//      }
//    }

//    std::cout << "past getline" << std::endl;

  }

  return 0;
}