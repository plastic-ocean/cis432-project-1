#include <netdb.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include "client.h"
#include "duckchat.h"
#include "raw.h"


// Client connects, logs in, and joins “Common”.
// Client reads lines from the user and parses commands.
// Client correctly sends Say message.
// Client uses select() to wait for input from the user and the server.
// Client correctly sends Join, Leave, Login, and Logout and handles Switch.
// Client correctly sends List and Who.


// Variables
int client_socket;
struct addrinfo *server_info;
std::string current_channel;
std::vector<std::string> kChannels;


/**
 * Prints an error message and exits the program.
 *
 * @msg is the error message to print.
 */
void Error(const char *msg) {
  std::cerr << msg << std::endl;
  exit(1);
}


/**
 * Prints the prompt.
 */
void PrintPrompt() {
  std::cout << "> " << std::flush;
}


/**
 * Clears the prompt.
 */
void ClearPrompt() {
  std::string backspaces = "";
  for (int i = 0; i < SAY_MAX; i++) {
    backspaces.append("\b");
  }
  std::cout << backspaces;
}


/**
 * Splits strings around spaces.
 *
 * @input is the input to split around.
 */
std::vector<std::string> StringSplit(std::string input) {
  std::istringstream iss(input);
  std::vector<std::string> result{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

  return result;
}


/**
 * Checks the address info of the server at a the given port. Prints error if not found.
 * getaddrinfo() returns a list of address structures into server_info_tmp.
 * Tries each address until a successful connect().
 * If socket() (or connect()) fails, closes the socket and tries the next address.
 *
 * @domain is the domain to connect to.
 * @port is the port to connect on.
 */
void CreateSocket(char *domain, const char *port) {
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


/**
 * Sends a message to all users in on the active channel.
 *
 * @message is the text message to send.
 */
int SendSay(std::string message) {
  if (message.size() > SAY_MAX) {
    std::cout << "Message exceeds the maximum message length." << std::endl;
    return 0;
  }

  struct request_say say;
  memset(&say, 0, sizeof(say));
  say.req_type = REQ_SAY;
  strncpy(say.req_text, message.c_str(), SAY_MAX);
  strncpy(say.req_channel, current_channel.c_str(), CHANNEL_MAX);

  if (sendto(client_socket, &say, sizeof(say), 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    Error("client: failed to send message\n");
  }

  return 0;
}


/**
 * Sends login requests to the server.
 *
 * @username is the name of the user to login.
 */
int SendLogin(char *username) {
  if (strlen(username) > USERNAME_MAX) {
    Error("User name exceeds the maximum username length.");
  }

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


/**
 * Sends logout request to the server.
 */
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


/**
 * Sends requests for the list of channels to the server.
 */
int SendList() {
  struct request_list list;
  list.req_type = REQ_LIST;
  size_t list_size = sizeof(struct request_list);

  if (sendto(client_socket, &list, list_size, 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    Error("client: failed to request list \n");
  }

  return 0;
}


/**
 * Sends requests for the list of users in the given channel.
 *
 * @channel is the channel to request users from.
 */
int SendWho(std::string channel) {
  struct request_who who;
  who.req_type = REQ_WHO;
  strncpy(who.req_channel, channel.c_str(), CHANNEL_MAX);
  size_t who_size = sizeof(struct request_who);

  if (sendto(client_socket, &who, who_size, 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    Error("client: failed to request list \n");
  }

  return 0;
}


/**
 * Sends requests to leave the given channel.
 *
 * @channel is the channel to leave.
 */
int SendLeave(std::string channel) {
  bool contains_channel = false;
  std::vector<std::string>::iterator it;
  for (it = kChannels.begin(); it != kChannels.end(); ++it) {
    if (*it == channel) {
      contains_channel = true;
      break;
    }
  }

  if (channel == current_channel) {
    current_channel = "";
  }

  struct request_leave leave;
  memset((char *) &leave, 0, sizeof(leave));
  leave.req_type = REQ_LEAVE;
  strncpy(leave.req_channel, channel.c_str(), CHANNEL_MAX);

  size_t leave_size = sizeof(struct request_leave);

  if (sendto(client_socket, &leave, leave_size, 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    Error("client: failed to request leave\n");
  }

  if (contains_channel) {
    kChannels.erase(it);
  }

  return 0;
}


/**
 * Switches to a channel the user has already joined.
 *
 * @channel is the channel to switch to.
 */
int SwitchChannel(std::string channel) {
  bool isSubscribed = false;

  if (kChannels.size() > 0) {
    for (auto c: kChannels) {
      if (channel == c) {
        current_channel = channel;
        isSubscribed = true;
      }
    }
  }

  if (!isSubscribed) {
    std::cout << "You have not subscribed to channel " << channel << std::endl;
  }

  return 0;
}


/**
 * Sends a join requests to the server.
 *
 * @channel is the channel to join.
 */
int SendJoin(std::string channel) {
  if (channel.size() > CHANNEL_MAX) {
    std::cout << "Channel name exceeds the maximum channel length." << std::endl;
    return 0;
  }

  bool contains_channel = false;
  for (std::vector<std::string>::iterator it = kChannels.begin(); it != kChannels.end(); ++it) {
    if (*it == channel) {
      contains_channel = true;
      break;
    }
  }

  if (!contains_channel) {
    kChannels.push_back(channel);

    struct request_join join;
    memset((char *) &join, 0, sizeof(join));
    join.req_type = REQ_JOIN;

    strncpy(join.req_channel, channel.c_str(), CHANNEL_MAX);

    size_t message_size = sizeof(struct request_join);

    if (sendto(client_socket, &join, message_size, 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
      Error("client: failed to request join\n");
    }
  }

  SwitchChannel(channel);

  return 0;
}


/**
 * Handles text_say packets from the server.
 *
 * @receive_buffer is the packet.
 * @output is the users input that must be rewritten back to the prompt.
 */
void HandleTextSay(char *receive_buffer, char *output) {
  struct text_say *say = (struct text_say *) receive_buffer;

  ClearPrompt();

  std::cout << "[" << say->txt_channel << "]" << "[" << say->txt_username << "]: " << say->txt_text << std::endl;

  PrintPrompt();

  std::cout << output << std::flush;
}


/**
 * Handles text_list packets from the server.
 *
 * @receive_buffer is the packet.
 * @output is the users input that must be rewritten back to the prompt.
 */
void HandleTextList(char *receive_buffer, char *output) {
  struct text_list *list = (struct text_list *) receive_buffer;

  ClearPrompt();

  std::cout << "Existing channels:" << std::endl;
  for (int i = 0; i < list->txt_nchannels; i++) {
    std::cout << " " << list->txt_channels[i].ch_channel << std::endl;
  }

  PrintPrompt();

  std::cout << output << std::flush;
}


/**
 * Handles text_who packets from the server.
 *
 * @receive_buffer is the packet.
 * @output is the users input that must be rewritten back to the prompt.
 */
void HandleTextWho(char *receive_buffer, char *output) {
  struct text_who *who = (struct text_who *) receive_buffer;

  ClearPrompt();

  std::cout << "Users on channel " << who->txt_channel << ":" << std::endl;
  for (int i = 0; i < who->txt_nusernames; i++) {
    std::cout << " " << who->txt_users[i].us_username << std::endl;
  }

  PrintPrompt();

  std::cout << output << std::flush;
}


/**
 * Handles error packets from the server.
 *
 * @receive_buffer is the packet.
 * @output is the users input that must be rewritten back to the prompt.
 */
void HandleError(char *receive_buffer, char *output) {
  struct text_error *error = (struct text_error *) receive_buffer;

  ClearPrompt();

  std::cout << "Error: " << error->txt_error << std::endl;

  PrintPrompt();

  std::cout << output << std::flush;
}


/**
 * Processes the input string to decide what type of command it is.
 *
 * @input is the string to process.
 */
bool ProcessInput(std::string input) {
  std::vector<std::string> inputs = StringSplit(input);

  if (inputs[0] == "/exit") {
    SendLogout();
    cooked_mode();
    return false;
  } else if (inputs[0] == "/list") {
    SendList();
  } else if (inputs[0] == "/join" && inputs.size() > 1) {
    SendJoin(inputs[1]);
  } else if (inputs[0] == "/leave" && inputs.size() > 1) {
    SendLeave(inputs[1]);
  } else if (inputs[0] == "/who" && inputs.size() > 1) {
    SendWho(inputs[1]);
  } else if (inputs[0] == "/switch" && inputs.size() > 1) {
    SwitchChannel(inputs[1]);
  } else {
    std::cout << "*Unknown command" << std::endl;
  }

  PrintPrompt();
  return true;
}


int main(int argc, char *argv[]) {
  char *domain;
  char *port_str;
  int port_num;
  char *username;
  std::string input;
  char *output = (char *) "";
  fd_set read_set;
  int result;

  char stdin_buffer[SAY_MAX + 1];
  char *stdin_buffer_pointer = stdin_buffer;

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

  current_channel = "Common";
  SendJoin(current_channel);

  if (raw_mode() != 0) {
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
        // User entered a char.
        char c = (char) getchar();
        if (c == '\n') {
          // Increments pointer and adds NULL char.
          *stdin_buffer_pointer++ = '\0';

          // Resets stdin_buffer_pointer to the start of stdin_buffer.
          stdin_buffer_pointer = stdin_buffer;

          std::cout << "\n" << std::flush;

          // Prevents output from printing on the new prompt after a newline char.
          output = (char *) "";

          input.assign(stdin_buffer, stdin_buffer + strlen(stdin_buffer));

          if (input[0] == '/') {
            if (!ProcessInput(input)) {
              break;
            }
          } else {
            // Sends chat messages.
            if (current_channel != "") {
              SendSay(input);
            } else {
              PrintPrompt();
            }
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
        ssize_t read_size = read(client_socket, receive_buffer, kBufferSize);

        if (read_size != 0) {
          struct text message;
          memcpy(&message, receive_buffer, sizeof(struct text));
          text_t text_type = message.txt_type;

          switch (text_type) {
            case TXT_SAY:
              HandleTextSay(receive_buffer, output);
              break;
            case TXT_LIST:
              HandleTextList(receive_buffer, output);
              break;
            case TXT_WHO:
              HandleTextWho(receive_buffer, output);
              break;
            case TXT_ERROR:
              HandleError(receive_buffer, output);
              break;
            default:
              break;
          }
        }

        memset(&receive_buffer, 0, SAY_MAX);
      } // end of if client_socket

    } // end of if result

  } // end of while

  free(server_info);
  close(client_socket);

  return 0;
}