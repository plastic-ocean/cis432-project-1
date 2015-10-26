#include <netdb.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include "client.h"
#include "duckchat.h"
#include "raw.h"

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


void PrintPrompt() {
  std::cout << ">" << std::flush;
}


// Splits strings around spaces.
std::vector<std::string> StringSplit(std::string input) {
  std::istringstream iss(input);
  std::vector<std::string> result{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

  return result;
}


// Splits strings around spaces.
std::vector<std::string> SplitString(char *input, char delimiter) {
  std::vector<std::string> result;
  std::string word = "";

  size_t input_size = strlen(input);
  for (size_t i = 0; i < input_size; i++) {
    if (input[i] != delimiter) {
      word += input[i];
    } else {
      result.push_back(word);
      word = "";
    }
  }
  result.push_back(word);

  return result;
}


void StripChar(char *input, char c) {
  size_t size = strlen(input);
  for (size_t i = 0; i < size; i++) {
    if (input[i] == c) {
      input[i] = '\0';
    }
  }
}


// Gets the address info of the server at a the given port and creates the client's socket.
void CreateSocket(char *domain, const char *port) {
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
  // Tries each address until a successful connect().
  // If socket() (or connect()) fails, closes the socket and tries the next address.

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
int SendSay(const char *message) {
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
int SendLogin(char *username) {
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
int SendLogout() {
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
int SendJoin(const char *channel) {
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


// Handles TXT-SAY server messages.
void HandleTextSay(char *receive_buffer, char *output) {
  struct text_say say;
  memcpy(&say, receive_buffer, sizeof(struct text_say));
  std::cout << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
  std::cout << "[" << say.txt_channel << "]" << "[" << say.txt_username << "]: " << say.txt_text << std::endl;
  PrintPrompt();
  std::cout << output << std::flush;
}


// Processes the input string to decide what type of command it is.
bool ProcessInput(std::string input) {
  std::vector<std::string> inputs = StringSplit(input);
  bool result = true;

  if (inputs[0] == "/exit") {
    SendLogout();
    cooked_mode();
    result = false;
  } else if (inputs[0] == "/list") {

  } else if (inputs[0] == "/join" && strlen(inputs) > 1) {
    SendJoin(inputs[1].c_str());
  } else if (inputs[0] == "/leave") {

  } else if (inputs[0] == "/who") {

  } else if (inputs[0] == "/switch") {

  } else {
    std::cout << "\n*Unknown command" << std::endl;
  }

  return result;
}


int main(int argc, char *argv[]) {
  char *domain;
  char *port_str;
  int port_num;
  char *username;
  char *input;
  char *output = (char *) "";

  char stdin_buffer[SAY_MAX + 1];
  char *stdin_buffer_pointer = stdin_buffer;

  fd_set read_set;
  int result;
  char receive_buffer[kBufferSize];
  memset(&receive_buffer, 0, kBufferSize);

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

  CreateSocket(domain, port_str);

  SendLogin(username);

  channel = (char *) "Common";
  SendJoin(channel);

  if (raw_mode() != 0){
    Error("client: error using raw mode");
  }

  PrintPrompt();

  while (1) {
    FD_ZERO(&read_set);
    FD_SET(client_socket, &read_set);
    FD_SET(STDIN_FILENO, &read_set);

    if ((result = select(client_socket + 1, &read_set, NULL, NULL, NULL)) < 0) {
      Error("client: problem using select");
    }

    if (result > 0) {
      if (FD_ISSET(STDIN_FILENO, &read_set)) {
        // User enter a char.
        char c = (char) getchar();
        if (c == '\n') {
          // Increments pointer and adds NULL char.
          *stdin_buffer_pointer++ = '\0';

          // Resets stdin_buffer_pointer to the start of stdin_buffer.
          stdin_buffer_pointer = stdin_buffer;

          std::cout << "\n" << std::flush;

          // Prevents output from printing on the new prompt after a newline char.
          output = (char *) "";

          input = stdin_buffer;
          if (input[0] == '/' && !ProcessInput(input)) {
            break;
          } else {
            // Sends chat messages.
            SendSay(input);
          }
        } else if (stdin_buffer_pointer != stdin_buffer + SAY_MAX) {
          // Increments pointer and adds char c.
          *stdin_buffer_pointer++ = c;
          
          std::cout << c << std::flush;

          // Copies pointer into output.
          output = stdin_buffer_pointer;
          // Increments and sets NULL char.
          *output++ = '\0';
          // Copies stdin_buffer into part of output before NULL char.
          output = stdin_buffer;
        }
      } else if (FD_ISSET(client_socket, &read_set)) {
        // Socket has data.
        int read_size = read(client_socket, receive_buffer, kBufferSize);

        if (read_size != 0) {
          struct text message;
          memcpy(&message, receive_buffer, sizeof(struct text));
          text_t text_type = message.txt_type;

          switch (text_type) {
            case TXT_SAY:
              HandleTextSay(receive_buffer, output);
              break;
            default:
              break;
          }
        }

        memset(&receive_buffer, 0, SAY_MAX);
      } // end of if client_socket

    } // end of if result

  } // end of while

  return 0;
}