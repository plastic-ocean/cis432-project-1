#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <cstring>
#include <stdio.h>
#include <string>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <netdb.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "server.h"
#include "duckchat.h"

// Server can accept connections.
// Server handles Login and Logout from users, and keeps records of which users are logged in.
// Server handles Join and Leave from users, keeps records of which channels a user belongs to,
// and keeps records of which users are in a channel.
// Server handles the Say message.
// Server correctly handles List and Who.
// Create copies of your client and server source. Modify them to send invalid packets to your good client
// and server, to see if you can make your client or server crash. Fix any bugs you find.

// TODO Leaving a non-existent channel should send and text_error
// TODO Who request on non-existent channel should print message server side and send text_error
// TODO add functions to header file


/**
 * A class used to keep track of a channel
 *
 * @name is the name of the channel
 * @users is a list of users currently in channel
 */
class Channel {
public:
  std::string name;
  std::list<std::shared_ptr<User>> users;

  Channel(std::string name): name(name) {};
};

/**
 * A class used to keep track of a user
 *
 * @name is the users name
 * @address is the users IP address
 * @port is the port the user is on
 */
class User {
public:
  std::string name;
  in_addr_t address;
  unsigned short port;

  User(std::string name, in_addr_t address, unsigned short port): name(name), address(address), port(port) {};
};


/* kUsers is a global map of all the users connected to the server */
std::map<std::string, std::shared_ptr<User>> users;
/* channels is a glbal map of all the channels that currently exist & have users in them */
std::map<std::string, std::shared_ptr<Channel>> channels;


/**
 * Prints an error message and exists.
 */
void Error(const char *message) {
  perror(message);
  exit(1);
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
  int client_socket;
  struct addrinfo *server_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;

  if ((status = getaddrinfo(domain, port, &hints, &server_info_tmp)) != 0) {
    std::cerr << "server: unable to resolve address: " << gai_strerror(status) << std::endl;
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
    Error("server: all sockets failed to connect");
  }
}


/**
 * Removes a users from every channel and from the global users list.
 *
 * @user is the user to remove.
 */
void RemoveUser(User *user) {
  for (auto channel : channels) {
    for (auto channel_user : channel.second->users) {
      if (channel_user->name == user->name) {
        channel.second->users.remove(channel_user);
        delete(user);
        break;
      }
    }
  }
  for (auto current_user : users) {
    if (current_user.second->name == user->name) {
      users.erase(user->name);
      delete(user);
    }
  }
}


/**
 * Logs in a user.
 *
 * @buffer is the login_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleLoginRequest(void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request_login login_request;
  memcpy(&login_request, buffer, sizeof(struct request_login));

  std::shared_ptr<User> current_user = std::make_shared<User>(login_request.req_username, request_address, request_port);
//  RemoveUser(current_user);
  users.insert({std::string(login_request.req_username), current_user});

  std::cout << "server: " << login_request.req_username << " logs in" << std::endl;
}


/**
 * Logs out a user.
 *
 * @buffer is the logout_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleLogoutRequest(void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request_logout logout_request;
  memcpy(&logout_request, buffer, sizeof(struct request_logout));

  for (auto user : users) {
    unsigned short current_port = user.second->port;
    in_addr_t current_address = user.second->address;

    if (current_port == request_port && current_address == request_address) {
      std::cout << "server: " << user.first << " logs out" << std::endl;
      
      users.erase(user.first);
      for (auto c : channels) {
        for (auto u : c.second->users) {
          if (u->name == user.first) {
            c.second->users.remove(u);
            break;
          }
        }
      }
      break;
    }
  }
}


/**
 * Adds a user to the requested channel.
 *
 * @buffer is the logout_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleJoinRequest(void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request_join join_request;
  memcpy(&join_request, buffer, sizeof(struct request_join));
  bool is_new_channel = true;
  bool is_channel_user;
  std::shared_ptr<Channel> channel;

  // If channel does exists in global map, set local channel to channel from channels
  for (auto ch : channels) {
    if (join_request.req_channel == ch.second->name) {
      is_new_channel = false;
      channel = ch.second;
      break;
    }
  }

  // If channel is new create it
  if (is_new_channel) {
    channel = std::make_shared<Channel>(join_request.req_channel);
  }

  for (auto user : users) {
    unsigned short current_port = user.second->port;
    in_addr_t current_address = user.second->address;
    if (current_port == request_port && current_address == request_address) {
      std::cout << "server: " << user.first << " joins channel "<< channel->name << std::endl;

      is_channel_user = false;
      for (auto u : channel->users) {
        if (u->name == user.second->name) {
          is_channel_user = true;
          break;
        }
      }

      if (!is_channel_user) {
        channel->users.push_back(user.second);
      }

      // Otherwise
      if (is_new_channel) {
        channels.insert({channel->name, channel});
      }
      break;
    }
  }
}


/**
 * Removes a user from the requested channel.
 *
 * @buffer is the logout_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleLeaveRequest(void *buffer, in_addr_t request_address, unsigned short request_port) {
  std::shared_ptr<Channel> channel;
  std::string current_channel;
  std::list<std::shared_ptr<User>>::const_iterator it;
  bool is_channel;
  struct request_leave leave_request;
  
  memcpy(&leave_request, buffer, sizeof(struct request_leave));
  current_channel = leave_request.req_channel;

  for (auto user : users) {
    unsigned short current_port = user.second->port;
    in_addr_t current_address = user.second->address;

    if (current_port == request_port && current_address == request_address) {
      is_channel = false;
      for (auto ch : channels) {
        if (ch.first == current_channel) {
          is_channel = true;
          break;
        }
      }

      if (is_channel) {
        channel = channels[current_channel];

        for (it = channel->users.begin(); it != channel->users.end(); ++it) {
          if ((*it)->name == user.first) {
            break;
          }
        }

        if (it != channel->users.end()) {
          channel->users.remove(*it);
          std::cout << user.first << " leaves channel " << channel->name << std::endl;
          if (channel->users.size() == 0) {
            channels.erase(channel->name);
            std::cout << "server: removing empty channel " << channel->name << std::endl;
          }
        }
        break;
      } else {
        std::cout << "server: " << user.first << " trying to leave non-existent channel " << current_channel << std::endl;
      }

    }
  }
}


/**
 * Sends message from a user to the requested channel.
 *
 * @server_socket is the socket to send on.
 * @buffer is the logout_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleSayRequest(int server_socket, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request_say say_request;
  memcpy(&say_request, buffer, sizeof(struct request_say));

  for (auto user : users) {
    unsigned short current_port = user.second->port;
    in_addr_t current_address = user.second->address;

    if (current_port == request_port && current_address == request_address) {
      for (auto channel_user : channels[say_request.req_channel]->users) {
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(struct sockaddr_in));

        struct text_say say;
        memcpy(&say, buffer, sizeof(struct text_say));

        client_addr.sin_family = AF_INET;
        client_addr.sin_port = channel_user->port;
        client_addr.sin_addr.s_addr = channel_user->address;

        // copy message into struct being sent
        strncpy(say.txt_channel, say_request.req_channel, CHANNEL_MAX);
        strncpy(say.txt_text, say_request.req_text, SAY_MAX);
        say.txt_type = TXT_SAY;
        strncpy(say.txt_username, user.second->name.c_str(), USERNAME_MAX);

        size_t message_size = sizeof(struct text_say);

        if (sendto(server_socket, &say, message_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
          Error("server: failed to send say\n");
        }
      }
      std::cout << user.second->name << " sends say message in " << say_request.req_channel << std::endl;
      break;
    }
  }
}


/**
 * Sends a text list packet containing every channel to the requesting user.
 *
 * @server_socket is the socket to send on.
 * @request_address is the address to send to.
 * @request_port is the port to send to.
 */
void HandleListRequest(int server_socket, in_addr_t request_address, unsigned short request_port) {
  struct sockaddr_in client_addr;
  size_t list_size = sizeof(text_list) + (channels.size() * sizeof(channel_info));
  struct text_list *list = (text_list *) malloc(list_size);
  memset(list, '\0', list_size);;

  list->txt_type = TXT_LIST;
  list->txt_nchannels = (int) channels.size();

  // Fills the packet's channels array.
  int i = 0;
  for (auto ch : channels) {
    strncpy(list->txt_channels[i++].ch_channel, ch.first.c_str(), CHANNEL_MAX);
  }

  // Finds the requesting users address and port and sends the packet.
  for (auto user : users) {
    unsigned short port = user.second->port;
    in_addr_t address = user.second->address;

    if (port == request_port && address == request_address) {
      memset(&client_addr, 0, sizeof(struct sockaddr_in));
      client_addr.sin_family = AF_INET;
      client_addr.sin_port = port;
      client_addr.sin_addr.s_addr = address;

      if (sendto(server_socket, list, list_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
        Error("server: failed to send list\n");
      }

      std::cout << "server: " << user.first << " lists channels" << std::endl;
      break;
    }
  }

  free(list);
}


/**
 * Sends a list of every user on the requested channel.
 *
 * @server_socket is the socket to send on.
 * @request_address is the address to send to.
 * @request_port is the port to send to.
 */
void HandleWhoRequest(int server_socket, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct sockaddr_in client_addr;
  struct request_who who_request;
  memcpy(&who_request, buffer, sizeof(struct request_who));

  int user_list_size = 0;

  for (auto c : channels) {
    if (c.first == who_request.req_channel) {
      user_list_size = (int) c.second->users.size();
    }
  }

  size_t who_size = sizeof(text_who) + (user_list_size * sizeof(user_info));
  struct text_who *who = (text_who *) malloc(who_size);
  memset(who, '\0', who_size);

  who->txt_type = TXT_WHO;
  strncpy(who->txt_channel, who_request.req_channel, CHANNEL_MAX);
  who->txt_nusernames = user_list_size;

  // Fills the packet's users array with the usernames.
  int i = 0;
  for (auto ch : channels) {
    if (ch.first == who_request.req_channel) {
      for (auto u : ch.second->users) {
        strncpy(who->txt_users[i++].us_username, u->name.c_str(), CHANNEL_MAX);
      }
      break;
    }
  }

  // Finds the requesting users address and port and sends the packet.
  for (auto user : users) {
    unsigned short port = user.second->port;
    in_addr_t address = user.second->address;

    if (port == request_port && address == request_address) {
      memset(&client_addr, 0, sizeof(struct sockaddr_in));
      client_addr.sin_family = AF_INET;
      client_addr.sin_port = port;
      client_addr.sin_addr.s_addr = address;

      if (sendto(server_socket, who, who_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
        Error("server: failed to send who\n");
      }

      std::cout << "server: " << user.first << " lists users in channel " << who_request.req_channel << std::endl;
      break;
    }
  }

  free(who);
}


/**
 * Processes a request.
 *
 * @server_socket is the socket to send on.
 * @buffer is the request
 * @request_address is the address to send to.
 * @request_port is the port to send to.
 */
void ProcessRequest(int server_socket, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request current_request;
  memcpy(&current_request, buffer, sizeof(struct request));
  request_t request_type = current_request.req_type;

  switch(request_type) {
    case REQ_LOGIN:
      HandleLoginRequest(buffer, request_address, request_port);
      break;
    case REQ_LOGOUT:
      HandleLogoutRequest(buffer, request_address, request_port);
      break;
    case REQ_JOIN:
      HandleJoinRequest(buffer, request_address, request_port);
      break;
    case REQ_LEAVE:
      HandleLeaveRequest(buffer, request_address, request_port);
      break;
    case REQ_SAY:
      HandleSayRequest(server_socket, buffer, request_address, request_port);
      break;
    case REQ_LIST:
      HandleListRequest(server_socket, request_address, request_port);
      break;
    case REQ_WHO:
      HandleWhoRequest(server_socket, buffer, request_address, request_port);
      break;
    default:
      break;
  }
}


int main(int argc, char *argv[]) {
  struct sockaddr_in server_addr;

  int server_socket;
  int receive_len;
  void* buffer[kBufferSize];
  int port;
  char *domain;
  const char *port_str;

  if (argc < 3) {
    std::cerr << "Usage: ./server domain_name port_num" << std::endl;
    exit(1);
  }

  domain = argv[1];
  port = atoi(argv[2]);
  port_str = argv[2];

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

  CreateSocket(domain, port_str);

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    receive_len = recvfrom(server_socket, buffer, kBufferSize, 0, (struct sockaddr *) &client_addr, &client_addr_len);

    if (receive_len > 0) {
      buffer[receive_len] = 0;

      ProcessRequest(server_socket, buffer, client_addr.sin_addr.s_addr, client_addr.sin_port);
    }
  }
}
