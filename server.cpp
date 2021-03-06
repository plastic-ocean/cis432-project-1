// CIS 432 Intro to Networks
// Programming Project 1 and 2
// Fall 2015
//
// Benjamin Barnes
// H. Keith Hamm


#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
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
#include <queue>
#include <netdb.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <random>
#include <climits>

#include "duckchat.h"

// Add the required debugging text for all messages received from clients.
// Add support to handle the additional command line arguments and setup the topology.
// Add support for broadcasting Joins when a user joins a channel.
// Add support for forwarding Joins from another server.
// Add support for Server-to-Server Say messages, including loop detection.
// Add support for sending Leave when a Say cannot be forwarded.
// Add support for the soft state features.
// Try several topologies. Verify that trees are formed and pruned correctly.
// Copy your server code and modify it to send invalid packets to see if you can make your server crash.
// ^ Fix any bugs you find.

#define UNUSED(x) (void)(x)


size_t kBufferSize = 2048;
unsigned int kTime = 60;


class Channel;
class User;
class Server;


void Error(std::string);
void HandleSigalarm(int);
unsigned int GetRandSeed();
unsigned long long GetRand();
std::shared_ptr<Channel> GetChannel(std::string);
void SendUsersS2SSay(Server, struct s2s_request_say);
void CreateServerChannel(std::string);
void CreateSocket(char *, const char *);
void SendS2SJoinRequest(Server, std::string, std::string);
void SendS2SSayRequest(Server, std::string, std::string, std::string, unsigned long long, std::string);
void SendS2SLeaveRequest(Server, std::string, std::string);
void HandleS2SJoinRequest(Server, void *, in_addr_t, unsigned short);
void HandleS2SSayRequest(Server, void *, in_addr_t, unsigned short);
void HandleError(int, std::string, std::string, in_addr_t, unsigned short);
void HandleLoginRequest(void *, in_addr_t , unsigned short );
void HandleLogoutRequest(void *, in_addr_t , unsigned short );
void HandleJoinRequest(void *, in_addr_t , unsigned short );
void HandleLeaveRequest(Server, void *, in_addr_t , unsigned short );
void HandleSayRequest(Server, void *, in_addr_t , unsigned short );
void HandleListRequest(Server, in_addr_t , unsigned short );
void HandleWhoRequest(Server, void *, in_addr_t , unsigned short );
void ProcessRequest(Server, void *, in_addr_t , unsigned short );


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


/**
 * A class used to keep track of a server.
 *
 * @host_name is the domain name for the server.
 * @port is the servers port.
 * @socket is socket this server talks on.
 * @ip is this server's IP address.
 * @channels is this server's channels.
 * @join_count is used to keep track of joins for handling alarms.
 */
class Server {
public:
  std::string host_name;
  std::string ip;
  int port;
  int socket;
  std::map<std::string, std::shared_ptr<Channel>> channels;
  int join_count;

  Server() {};

  Server(std::string host_name, char *port, int socket): host_name(host_name), port(atoi(port)), socket(socket) {
    struct hostent *he;
    struct in_addr **addr_list;

    if ((he = gethostbyname(host_name.c_str())) == NULL) {
      Error("Failed to resolve hostname " + host_name);
    }

    addr_list = (struct in_addr **) he->h_addr_list;
    ip = std::string(inet_ntoa(*addr_list[0]));

    join_count = 0;
  };
};


/* all the users connected to the server; key = username */
std::map<std::string, std::shared_ptr<User>> users;

/* all the channels that currently exist & have users in them; key = channel_name */
std::map<std::string, std::shared_ptr<Channel>> user_channels;

/* all adjacent servers; key = "ip:port" */
std::map<std::string, std::shared_ptr<Server>> servers;

/* all the server channels for this server; key = channel name */
std::map<std::string, std::shared_ptr<Channel>> server_channels;

/* the S2S say unique ID cache */
std::deque<unsigned long long> s2s_say_cache;

/* this server's info object */
Server server;

/* choose a linear congruential engine */
std::minstd_rand rand_generator;

/* define our uniform distribution and range */
std::uniform_int_distribution<unsigned long long> distribution(1, ULLONG_MAX);


/**
 * Prints an error message and exists.
 *
 * @message is the error to print.
 */
void Error(std::string message) {
  std::cerr << message << std::endl;
  exit(1);
}


/**
 * Handles alarms going off for sending S2S joins.
 *
 * @sig is the unused signal.
 */
void HandleSigalarm(int sig) {
  UNUSED(sig);
  signal(SIGALRM, &HandleSigalarm);
  alarm(0);
  alarm(kTime);

  // Checking if a joins has been received from all servers in the network.
  for (auto adj_server : servers) {
    if (adj_server.second->join_count <= 0) {
      // Remove all channels from server as if it left.
      adj_server.second->channels.clear();
    } else {
      adj_server.second->join_count--;
    }
  }

  // Sending join for all our channels to all adjacent servers.
  for (auto c : server_channels) {
    SendS2SJoinRequest(server, c.first, "");
  }
}


/**
 * Gets a random seeds used for timers.
 *
 * @return the random seed.
 */
unsigned int GetRandSeed() {
  unsigned int random_seed;
  std::ifstream file("/dev/urandom", std::ios::binary);
  
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
 * Generates a random 64 bit unsigned long long based of the previously seeded rand_generator.
 *
 * @return the random number.
 */
unsigned long long GetRand() {
  unsigned long long number = distribution(rand_generator);

  return number;
}


/**
 * Sends a S2S say to all local users.
 *
 * @server is this server.
 * @s2s_say is the received S2S say that is send to users.
 */
void SendUsersS2SSay(Server server, struct s2s_request_say s2s_say) {
  for (auto channel_user : user_channels[s2s_say.req_channel]->users) {
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = channel_user->port;
    client_addr.sin_addr.s_addr = channel_user->address;

    struct text_say say;
    say.txt_type = TXT_SAY;
    strncpy(say.txt_channel, s2s_say.req_channel, CHANNEL_MAX);
    strncpy(say.txt_text, s2s_say.req_text, SAY_MAX);
    strncpy(say.txt_username, s2s_say.req_username, USERNAME_MAX);

    size_t message_size = sizeof(struct text_say);

    if (sendto(server.socket, &say, message_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
      Error("Failed to send say\n");
    }
  }
}


/**
 * Gets a channel from the channels map or creates a new channel.
 *
 * @name is the name of the channel to get
 */
std::shared_ptr<Channel> GetChannel(std::string name) {
  bool is_new_channel = true;
  std::shared_ptr<Channel> channel;

  // If channel already exists, get it from channels map
  for (auto ch : user_channels) {
    if (name == ch.second->name) {
      is_new_channel = false;
      channel = ch.second;
      break;
    }
  }

  // If channel is new create it
  if (is_new_channel) {
    channel = std::make_shared<Channel>(std::string(name));
    user_channels.insert({channel->name, channel});
  }

  return channel;
}


/**
 * Gets a channel from the server channels map or creates a new channel.
 *
 * @name is the name of the channel to get.
 */
void CreateServerChannel(std::string name) {
  bool is_new_channel = true;
  std::shared_ptr<Channel> channel;

  // If channel already exists, get it from channels map
  for (auto ch : server_channels) {
    if (name == ch.second->name) {
      is_new_channel = false;
      channel = ch.second;
      break;
    }
  }

  // If channel is new create it
  if (is_new_channel) {
    channel = std::make_shared<Channel>(std::string(name));
    server_channels.insert({channel->name, channel});
  }
}


/**
 * Sends a S2S Join to all adjacent servers.
 *
 * @server is this server's info.
 * @channel is the channel to send to other servers.
 */
void SendS2SJoinRequest(Server server, std::string channel_name, std::string request_ip_port) {
  size_t servers_size = servers.size();

  if (servers_size > 0) {
    struct s2s_request_join join;
    memcpy(join.req_channel, channel_name.c_str(), CHANNEL_MAX);
    join.req_type = REQ_S2S_JOIN;

    size_t message_size = sizeof(struct s2s_request_join);

    for (auto adj_server : servers) {
      std::string adj_server_ip_port = adj_server.second->ip + ":" + std::to_string(adj_server.second->port);
      if (adj_server_ip_port != request_ip_port) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(struct sockaddr_in));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(adj_server.second->port);
        inet_pton(AF_INET, adj_server.second->ip.c_str(), &server_addr.sin_addr.s_addr);

        // Add adj_server to channel_name.
        std::shared_ptr<Channel> channel = std::make_shared<Channel>(std::string(channel_name));
        adj_server.second->channels.insert({std::string(channel_name), channel});

        if (sendto(server.socket, &join, message_size, 0, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
          Error("Failed to send S2S Join\n");
        }

        std::cout << server.ip << ":" << server.port << " " << adj_server.second->ip << ":" << adj_server.second->port
        << " send S2S Join " << channel_name << std::endl;
      }
    }
  }
}


/**
 * Sends a S2S Say to all adjacent servers.
 *
 * @server is this server's info.
 * @username is the sending user.
 * @text is the message to send.
 * @channel is the channel to send to other servers.
 * @unique_id is the message's unique id.
 * @request_ip_port is the ip and port string of the requesting server.
 */
void SendS2SSayRequest(Server server, std::string username, std::string text, std::string channel,
                       unsigned long long unique_id, std::string request_ip_port) {
  struct s2s_request_say say;
  strncpy(say.req_channel, channel.c_str(), CHANNEL_MAX);
  strncpy(say.req_username, username.c_str(), USERNAME_MAX);
  strncpy(say.req_text, text.c_str(), SAY_MAX);
  say.req_type = REQ_S2S_SAY;
  size_t message_size = sizeof(struct s2s_request_say);

  if (unique_id == 0) {
    say.uniq_id = GetRand(); // rand();
  } else {
    say.uniq_id = unique_id;
  }

  for (auto adj_server : servers) {
    std::string adj_server_ip_port = adj_server.second->ip + ":" + std::to_string(adj_server.second->port);
    if (adj_server_ip_port != request_ip_port &&
        adj_server.second->channels.find(channel) != adj_server.second->channels.end()) {
      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(struct sockaddr_in));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(adj_server.second->port);
      inet_pton(AF_INET, adj_server.second->ip.c_str(), &server_addr.sin_addr.s_addr);

      if (sendto(server.socket, &say, message_size, 0, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        Error("Failed to send S2S Say\n");
      }

      std::cout << server.ip << ":" << server.port << " " << adj_server.second->ip << ":" << adj_server.second->port
      << " send S2S Say " << username << " " << channel << " \"" << text << "\"" << std::endl;
    }
  }
}


/**
 * Sends a S2S Leave to all adjacent servers.
 *
 * @server is this server's info.
 * @channel is the channel to send to other servers.
 */
void SendS2SLeaveRequest(Server server, std::string channel, std::string request_ip_port) {
  size_t servers_size = servers.size();

  server_channels.erase(channel);

  if (servers_size > 0) {
    struct s2s_request_leave leave;
    memcpy(leave.req_channel, channel.c_str(), CHANNEL_MAX);
    leave.req_type = REQ_S2S_LEAVE;

    size_t message_size = sizeof(struct s2s_request_leave);

    if (request_ip_port == "") {
      for (auto adj_server : servers) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(struct sockaddr_in));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(adj_server.second->port);
        inet_pton(AF_INET, adj_server.second->ip.c_str(), &server_addr.sin_addr.s_addr);

        if (sendto(server.socket, &leave, message_size, 0, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
          Error("Failed to send S2S Leave\n");
        }

        std::cout << server.ip << ":" << server.port << " " << adj_server.second->ip << ":" << adj_server.second->port
        << " send S2S Leave " << channel << std::endl;
      }
    } else {
      std::shared_ptr<Server> adj_server = servers[request_ip_port];

      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(struct sockaddr_in));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(adj_server->port);
      inet_pton(AF_INET, adj_server->ip.c_str(), &server_addr.sin_addr.s_addr);

      if (sendto(server.socket, &leave, message_size, 0, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        Error("Failed to send S2S Leave\n");
      }

      std::cout << server.ip << ":" << server.port << " " << request_ip_port
      << " send S2S Leave " << channel << std::endl;
    }
  }
}


/**
 * Handles S2S join requests.
 *
 * @server is this server.
 * @buffer is the login_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleS2SJoinRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct s2s_request_join *join = (struct s2s_request_join *) buffer;
  char request_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &request_address, request_ip, INET_ADDRSTRLEN);

  std::string request_ip_port = std::string(request_ip) + ":" + std::to_string(ntohs(request_port));

  std::cout << server.ip << ":" << server.port << " " << request_ip_port
  << " recv S2S Join " << join->req_channel << std::endl;

  // Add requester to channel.
  std::shared_ptr<Channel> channel = std::make_shared<Channel>(std::string(join->req_channel));
  servers.find(request_ip_port)->second->channels.insert({std::string(join->req_channel), channel});

  // After 2 minutes expect to receive join from each server, this keeps the count at 2.
  if (servers.find(request_ip_port)->second->join_count != 2) {
    servers.find(request_ip_port)->second->join_count = 2;
  }

  if (server_channels.find(join->req_channel) == server_channels.end()) {
    CreateServerChannel(join->req_channel);
    SendS2SJoinRequest(server, join->req_channel, request_ip_port);
  }
}


/**
 * Handles S2S Say requests.
 *
 * @server is this server.
 * @buffer is the login_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleS2SSayRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct s2s_request_say *say = (struct s2s_request_say *) buffer;
  char request_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &request_address, request_ip, INET_ADDRSTRLEN);
  bool is_in_cache = false;
  size_t servers_size = servers.size();
  std::string request_ip_port = std::string(request_ip) + ":" + std::to_string(ntohs(request_port));

  // check the cache
  size_t cache_size = s2s_say_cache.size();
  for (auto uniq_id : s2s_say_cache) {
    if (uniq_id == say->uniq_id) {
      is_in_cache = true;
    }
  }

  if (!is_in_cache) {
    std::cout << server.ip << ":" << server.port << " " << request_ip_port
    << " recv S2S Say " << say->req_username << " " << say->req_channel << " \"" << say->req_text << "\"" << std::endl;

    if (servers_size > 0) {
      SendS2SSayRequest(server, say->req_username, say->req_text, say->req_channel, say->uniq_id, request_ip_port);
    }

    if (cache_size == 3) {
      s2s_say_cache.pop_front();
    }
    s2s_say_cache.push_back(say->uniq_id);

    // if the s2s say req channel is in user channels, send message to users
    if (user_channels.find(say->req_channel) != user_channels.end()) {
      SendUsersS2SSay(server, *say);
    }

    int active_servers = 0;
    for (auto s : servers) {
      if (s.second->channels.size() > 0) {
        active_servers++;
      }
    }

    // if there is only one adjacent server and no users
    if (active_servers == 1 && (user_channels.find(say->req_channel) == user_channels.end())) {
      SendS2SLeaveRequest(server, say->req_channel, "");
    }
  } else {
    // send leave only to sender
    SendS2SLeaveRequest(server, say->req_channel, request_ip_port);
  }
}


/**
 * Handles S2S Leave requests.
 *
 * @server is this server.
 * @buffer is the login_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleS2SLeaveRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct s2s_request_leave *leave = (struct s2s_request_leave *) buffer;
  char request_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &request_address, request_ip, INET_ADDRSTRLEN);

  std::string request_ip_port = std::string(request_ip) + ":" + std::to_string(ntohs(request_port));

  std::cout << server.ip << ":" << server.port << " " << request_ip_port
  << " recv S2S Leave " << leave->req_channel << std::endl;

  // Remove requester from server channel.
  servers.find(request_ip_port)->second->channels.erase(leave->req_channel);
}


/**
 * Handles errors from leave or who requests.
 *
 * @server_socket is the socket to send on.
 * @channel is the channel's name
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleError(int server_socket, std::string channel, std::string type, in_addr_t request_address,
                 unsigned short request_port) {
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
        Error("Failed to send error\n");
      }

      if (type == "who") {
        message_type = message_who;
      } else {
        message_type = message_leave;
      }

      std::cout << user.first << " trying to " << message_type << " non-existent channel " << channel
      << std::endl;

      break;
    }
  }
}


/**
 * Logs in a user.
 *
 * @server is this server.
 * @buffer is the login_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleLoginRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request_login login_request;
  memcpy(&login_request, buffer, sizeof(struct request_login));

  char request_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &request_address, request_ip, INET_ADDRSTRLEN);

  std::shared_ptr<User> current_user = std::make_shared<User>(std::string(request_ip), login_request.req_username,
                                                              request_address, request_port);
  users.insert({std::string(login_request.req_username), current_user});

  std::cout << server.ip << ":" << server.port << " " << request_ip << ":" << request_port
  << " recv Request Login " << login_request.req_username << std::endl;
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
      std::cout << server.ip << ":" << server.port << " " <<  user.first << " logs out" << std::endl;
      
      users.erase(user.first);
      for (auto c : user_channels) {
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
 * @server is this server.
 * @buffer is the logout_request
 * @request_address is the user's address.
 * @request_port is the user's port.
 */
void HandleJoinRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct request_join join_request;
  memcpy(&join_request, buffer, sizeof(struct request_join));
  bool is_channel_user;
  std::shared_ptr<Channel> channel = GetChannel(join_request.req_channel);

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

      break;
    }
  }

  if (server_channels.find(channel->name) == server_channels.end()) {
    CreateServerChannel(channel->name);
    SendS2SJoinRequest(server, channel->name, "");
  }
}


/**
 * Removes a user from the requested channel.
 *
 * @server is this server.
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
      for (auto ch : user_channels) {
        if (ch.first == current_channel) {
          is_channel = true;
          break;
        }
      }

      if (is_channel) {
        channel = user_channels[current_channel];

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
            user_channels.erase(channel->name);
            std::cout << "removing empty channel " << channel->name << std::endl;
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
 * @server is this server.
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
      for (auto channel_user : user_channels[say_request.req_channel]->users) {
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
          Error("Failed to send say\n");
        }
      }

      char request_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &request_address, request_ip, INET_ADDRSTRLEN);

      std::string request_ip_port = std::string(request_ip) + ":" + std::to_string(ntohs(request_port));

      std::cout << server.ip << ":" << server.port << " " << request_ip_port
      << " recv Request say " << user.first << " " << say_request.req_channel
      << " \"" << say_request.req_text << "\"" << std::endl;

      size_t servers_size = servers.size();
      if (servers_size > 0) {
        SendS2SSayRequest(server, user.first, say_request.req_text, say_request.req_channel, 0, request_ip_port);
      }

      break;
    }
  }


}


/**
 * Sends a text list packet containing every channel to the requesting user.
 *
 * @server is this server.
 * @request_address is the address to send to.
 * @request_port is the port to send to.
 */
void HandleListRequest(Server server, in_addr_t request_address, unsigned short request_port) {
  struct sockaddr_in client_addr;
  size_t list_size = sizeof(text_list) + (user_channels.size() * sizeof(channel_info));
  struct text_list *list = (text_list *) malloc(list_size);
  memset(list, '\0', list_size);;

  list->txt_type = TXT_LIST;
  list->txt_nchannels = (int) user_channels.size();

  // Fills the packet's channels array.
  int i = 0;
  for (auto ch : user_channels) {
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
        Error("Failed to send list\n");
      }

      std::cout << server.ip << ":" << server.port << " " << user.first << " list channels" << std::endl;
      break;
    }
  }

  free(list);
}


/**
 * Sends a list of every user on the requested channel.
 *
 * @server is this server.
 * @request_address is the address to send to.
 * @request_port is the port to send to.
 */
void HandleWhoRequest(Server server, void *buffer, in_addr_t request_address, unsigned short request_port) {
  struct sockaddr_in client_addr;
  struct request_who who_request;
  memcpy(&who_request, buffer, sizeof(struct request_who));

  int user_list_size = 0;

  bool is_channel = false;
  for (auto c : user_channels) {
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
  for (auto ch : user_channels) {
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
        Error("Failed to send who\n");
      }

      std::cout << server.ip << ":" << server.port << " " << user.first << " lists users in channel " << who_request.req_channel << std::endl;
      break;
    }
  }

  free(who);
}


/**
 * Processes a request.
 *
 * @server is this server.
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
    case REQ_S2S_SAY:
      HandleS2SSayRequest(server, buffer, request_address, request_port);
      break;
    case REQ_S2S_LEAVE:
      HandleS2SLeaveRequest(server, buffer, request_address, request_port);
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

//  srand(GetRandSeed());

  rand_generator.seed(GetRandSeed()); // create a seed

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
    Error("Failed to open socket\n");
  }

  if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    Error("Failed to bind socket\n");
  }

  server = Server(domain, port, server_socket);

  // Adds adjacent servers to servers map.
  for (int i = 3; i < argc; i += 2) {
    std::shared_ptr<Server> adj_server = std::make_shared<Server>(std::string(argv[i]), argv[i + 1], -1);
    std::string key = adj_server->ip + ":" + std::to_string(adj_server->port);
    servers.insert({key, adj_server});
  }

  for (auto adj_server : servers) {
    std::cout << server.ip << ":" << server.port << " " << adj_server.second->ip << ":" << adj_server.second->port
    << " connected" << std::endl;
  }

  signal(SIGALRM, &HandleSigalarm);
  alarm(kTime);

  while (1) {
    struct sockaddr_in sock_addr;
    socklen_t addr_len = sizeof(sock_addr);
    receive_len = (int) recvfrom(server.socket, buffer, kBufferSize, 0, (struct sockaddr *) &sock_addr, &addr_len);

    if (receive_len > 0) {
      buffer[receive_len] = 0;

      ProcessRequest(server, buffer, sock_addr.sin_addr.s_addr, sock_addr.sin_port);
    }
  }
}
