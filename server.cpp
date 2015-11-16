// CIS 432 Into to Networks
// Programming Project 1
// Fall 2015
//
// Benjamin Barnes
// H. Keith Hamm


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
#include <arpa/inet.h>
#include <fstream>

#include "server.h"
#include "duckchat.h"

// Add the required debugging text for all messages received from clients.
// Add support to handle the additional command line arguments and setup the topology.
// Add support for broadcasting Joins when a user joins a channel.
// TODO Add support for forwarding Joins from another server.
// TODO Add support for Server-to-Server Say messages, including loop detection.
// TODO Add support for sending Leave when a Say cannot be forwarded.
// TODO Add support for the soft state features.
// TODO Try several topologies. Verify that trees are formed and pruned correctly.
// TODO Copy your server code and modify it to send invalid packets to see if you can make your server crash. Fix any bugs you find.


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
  std::string ip;
  std::string name;
  in_addr_t address;
  unsigned short port;

  User(std::string ip, std::string name, in_addr_t address, unsigned short port): ip(ip), name(name), address(address),
                                                                                  port(port) {};
};


class Server {
public:
  std::string host_name;
  std::string ip;
  int port;
  int socket;
  
  Server() {};

  Server(std::string host_name, char *port, int socket): host_name(host_name), port(atoi(port)), socket(socket) {
    struct hostent *he;
    struct in_addr **addr_list;

    if ((he = gethostbyname(host_name.c_str())) == NULL) {
      Error("server: error resolving hostname " + host_name);
    }

    addr_list = (struct in_addr **) he->h_addr_list;
    ip = std::string(inet_ntoa(*addr_list[0]));
  };
};


/* users is all the users connected to the server */
std::map<std::string, std::shared_ptr<User>> users;

/* channels is all the channels that currently exist & have users in them */
std::map<std::string, std::shared_ptr<Channel>> channels;

/* servers is all adjacent servers */
std::map<std::string, std::shared_ptr<Server>> servers;


/**
 * Prints an error message and exists.
 */
void Error(std::string message) {
  std::cerr << message << std::endl;
  exit(1);
}


unsigned int GetRandSeed() {
  unsigned int random_seed;
  std::ifstream file("/dev/urandom", std::ios::binary);
  std::cout << "getting rand seed" << std::endl;
  
  if (file.is_open()) {
    char * temp_block;
    int size = sizeof(int);
    temp_block = new char[size];
    file.read(temp_block, size);
    file.close();
    random_seed = static_cast<unsigned int>(*temp_block);
    delete[] temp_block;
  } else {
    random_seed = 0;
    Error("Failed to read /dev/urandom");
  }

  return random_seed;
}


/**
 * Sends a Join message to all adjacent servers.
 *
 * @server is this server's info.
 * @channel is the channel to send to other servers.
 */
void SendS2SJoinRequest(Server server, std::string channel) {
  size_t servers_size = servers.size();

  if (servers_size > 0) {
    struct s2s_request_join join;
    memcpy(join.req_channel, channel.c_str(), sizeof(channel));
    join.req_type = REQ_S2S_JOIN;

    size_t message_size = sizeof(struct s2s_request_join);

    for (auto adj_server : servers) {
      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(struct sockaddr_in));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(adj_server.second->port);
      inet_pton(AF_INET, adj_server.second->ip.c_str(), &server_addr.sin_addr.s_addr);

      if (sendto(server.socket, &join, message_size, 0, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        Error("server: failed to send s2s join\n");
      }

      std::cout << server.ip << ":" << server.port << " " << adj_server.second->ip << ":" << adj_server.second->port
      << " send S2S Join " << channel << std::endl;
    }
  }

}


void HandleS2SJoinRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct s2s_request_join *join = (struct s2s_request_join *) buffer;
  char request_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &request_address, request_ip, INET_ADDRSTRLEN);

  std::cout << server.ip << ":" << server.port << " " << request_ip << ":" << ntohs(request_port)
  << " recv S2S Join " << join->req_channel << std::endl;

  // TODO if this server is not already subscribed to channel: subscribe to channel and forward the message

  // TODO don't send s2s join to the server that sent us the request otherwise endless loop!
//  SendS2SJoinRequest(server, join->req_channel);
}


/**
 * Handles errors from leave or who requests.
 *
 * @server_socket is the socket to send on.
 * @channel is the channel's name
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleError(int server_socket, std::string channel, std::string type, in_addr_t request_address, unsigned short request_port) {
  std::string message_who = "list users in";
  std::string message_leave = "leave";
  std::string message_type = "";

  for (auto user : users) {
    unsigned short current_port = user.second->port;
    in_addr_t current_address = user.second->address;

    if (current_port == request_port && current_address == request_address) {
      struct sockaddr_in client_addr;
      memset(&client_addr, 0, sizeof(struct sockaddr_in));

      struct text_error error;

      client_addr.sin_family = AF_INET;
      client_addr.sin_port = current_port;
      client_addr.sin_addr.s_addr = current_address;

      std::string message = "No channel by the name " + channel;
      strncpy(error.txt_error, message.c_str(), SAY_MAX);
      error.txt_type = TXT_ERROR;

      size_t message_size = sizeof(struct text_error);

      if (sendto(server_socket, &error, message_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
        Error("server: failed to send error\n");
      }

      if (type == "who") {
        message_type = message_who;
      } else {
        message_type = message_leave;
      }

      std::cout << "server: " << user.first << " trying to " << message_type << " non-existent channel " << channel << std::endl;

      break;
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
void HandleLoginRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request_login login_request;
  memcpy(&login_request, buffer, sizeof(struct request_login));

  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &request_address, ip, INET_ADDRSTRLEN);

  std::shared_ptr<User> current_user = std::make_shared<User>(std::string(ip), login_request.req_username, request_address, request_port);
  users.insert({std::string(login_request.req_username), current_user});

  std::cout << server.ip << ":" << server.port << " " << ip << ":"
  << request_port << " recv Request login " << login_request.req_username << std::endl;
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
void HandleJoinRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
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
    channel = std::make_shared<Channel>(std::string(join_request.req_channel));
  }

  for (auto user : users) {
    unsigned short current_port = user.second->port;
    in_addr_t current_address = user.second->address;
    if (current_port == request_port && current_address == request_address) {
      std::cout << server.ip << ":" << server.port << " " << user.second->ip << ":"
      << request_port << " recv Request join " << user.first << " " << channel->name << std::endl;

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

  // TODO if server is not subscribed to channel: subscribe to the channel then sends2sjoinrequest
  SendS2SJoinRequest(server, channel->name);
}


/**
 * Removes a user from the requested channel.
 *
 * @buffer is the logout_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleLeaveRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
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
          std::cout << server.ip << ":" << server.port << " " << user.second->ip << ":"
          << request_port << " recv Request leave " << user.first << " " << channel->name << std::endl;
          if (channel->users.size() == 0) {
            channels.erase(channel->name);
            std::cout << "server: removing empty channel " << channel->name << std::endl;
          }
        }
        break;
      } else {
        HandleError(server.socket, std::string(current_channel), "leave", request_address, request_port);
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
void HandleSayRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
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

        if (sendto(server.socket, &say, message_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
          Error("server: failed to send say\n");
        }
      }
      std::cout << server.ip << ":" << server.port << " " << user.second->ip << ":"
      << request_port << " recv Request say " << user.first << " " << say_request.req_channel
      << " \"" << say_request.req_text << "\"" << std::endl;
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
void HandleListRequest(Server server, in_addr_t request_address, unsigned short request_port) {
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

      if (sendto(server.socket, list, list_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
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
void HandleWhoRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct sockaddr_in client_addr;
  struct request_who who_request;
  memcpy(&who_request, buffer, sizeof(struct request_who));

  int user_list_size = 0;

  bool is_channel = false;
  for (auto c : channels) {
    if (c.first == who_request.req_channel) {
      user_list_size = (int) c.second->users.size();
      is_channel = true;
    }
  }

  if (!is_channel) {
    // send error
    HandleError(server.socket, std::string(who_request.req_channel), "who", request_address, request_port);
    return;
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

      if (sendto(server.socket, who, who_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
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
void ProcessRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request current_request;
  memcpy(&current_request, buffer, sizeof(struct request));
  request_t request_type = current_request.req_type;

  switch(request_type) {
    case REQ_LOGIN:
      HandleLoginRequest(server, buffer, request_address, request_port);
      break;
    case REQ_LOGOUT:
      HandleLogoutRequest(buffer, request_address, request_port);
      break;
    case REQ_JOIN:
      HandleJoinRequest(server, buffer, request_address, request_port);
      break;
    case REQ_LEAVE:
      HandleLeaveRequest(server, buffer, request_address, request_port);
      break;
    case REQ_SAY:
      HandleSayRequest(server, buffer, request_address, request_port);
      break;
    case REQ_LIST:
      HandleListRequest(server, request_address, request_port);
      break;
    case REQ_WHO:
      HandleWhoRequest(server, buffer, request_address, request_port);
      break;
    case REQ_S2S_JOIN:
      HandleS2SJoinRequest(server, buffer, request_address, request_port);
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
  char *domain;
  char *port;

  if (argc < 3) {
    std::cerr << "Usage: ./server domain_name port_num" << std::endl;
    exit(1);
  }

  domain = argv[1];
  port = argv[2];

  memset((char *) &server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(atoi(port));

  if ((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    Error("server: can't open socket\n");
  }

  if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    Error("server: bind failed\n");
  }

  Server server = Server(domain, port, server_socket);

  for (int i = 3; i < argc; i += 2) {
    std::shared_ptr<Server> adj_server = std::make_shared<Server>(std::string(argv[i]), argv[i + 1], -1);
    std::string key = adj_server->ip + ":" + std::to_string(adj_server->port);
    servers.insert({key, adj_server});
  }

  for (auto adj_server : servers) {
    std::cout << server.ip << ":" << server.port << adj_server.second->ip << ":" << adj_server.second->port
    << " connected" << std::endl;
  }

  while (1) {
    struct sockaddr_in sock_addr;
    socklen_t addr_len = sizeof(sock_addr);
    receive_len = recvfrom(server.socket, buffer, kBufferSize, 0, (struct sockaddr *) &sock_addr, &addr_len);

    if (receive_len > 0) {
      buffer[receive_len] = 0;

      ProcessRequest(server, buffer, sock_addr.sin_addr.s_addr, sock_addr.sin_port);
    }
  }
}
