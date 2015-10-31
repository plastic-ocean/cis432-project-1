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

#include "server.h"
#include "duckchat.h"

// TODO Handle domain
// Server can accept connections.
// Server handles Login and Logout from users, and keeps records of which users are logged in.
// Server handles Join and Leave from users, keeps records of which channels a user belongs to,
// and keeps records of which users are in a channel.
// TODO Server handles the Say message.
// TODO Server correctly handles List and Who.
// TODO Create copies of your client and server source. Modify them to send invalid packets to your good client
// and server, to see if you can make your client or server crash. Fix any bugs you find.


//std::string user;

class Channel {
public:
  std::string name;
  std::list<User *> users;

  Channel(std::string name): name(name) {};
};

class User {
public:
  std::string name;
  in_addr_t address;
  unsigned short port;
//  std::set<Channel *> channels;

  User(std::string name, in_addr_t address, unsigned short port): name(name), address(address), port(port) {};
};


struct sockaddr_in most_recent_client_addr;
std::map<std::string, User *> kUsers;
std::map<std::string, Channel *> kChannels;


void Error(const char *msg) {
  perror(msg);
  exit(1);
}


void ProcessRequest(int server_socket, void *buffer, in_addr_t user_address, unsigned short user_port) {
  struct request current_request;
  User *current_user;
  Channel *channel;
  std::string current_channel;
  std::list<User *>::const_iterator it;
  bool is_new_channel;
  bool is_channel;

  memcpy(&current_request, buffer, sizeof(struct request));
//  std::cout << "request type: " << current_request.req_type << std::endl;
  request_t request_type = current_request.req_type;

  switch(request_type) {
    case REQ_LOGIN:
      struct request_login login_request;
      memcpy(&login_request, buffer, sizeof(struct request_login));

      current_user = new User(login_request.req_username, user_address, user_port);
      kUsers.insert({std::string(login_request.req_username), current_user});

      std::cout << "server: " << login_request.req_username << " logs in" << std::endl;
      break;

    case REQ_LOGOUT:
      struct request_logout logout_request;
      memcpy(&logout_request, buffer, sizeof(struct request_logout));

      for (auto user : kUsers) {
        unsigned short current_port = user.second->port;
        in_addr_t current_address = user.second->address;

        if (current_port == user_port && current_address == user_address) {
          std::cout << "server: " << user.first << " logs out" << std::endl;
          kUsers.erase(user.first);
          break;
        }
      }
      break;

    case REQ_LEAVE:
      struct request_leave leave_request;
      memcpy(&leave_request, buffer, sizeof(struct request_leave));
      current_channel = leave_request.req_channel;
      for (auto user : kUsers) {
        unsigned short current_port = user.second->port;
        in_addr_t current_address = user.second->address;

        if (current_port == user_port && current_address == user_address) {
          is_channel = false;
          for(auto ch : kChannels){
            if(ch.first == current_channel){
              std::cout << "channel found" << std::endl;
              is_channel = true;
              break;
            }
          }

          if(is_channel){
            channel = kChannels[current_channel];

            for (it = channel->users.begin(); it != channel->users.end(); ++it){
              if((*it)->name == user.first){
                break;
              }
            }

            if( it != channel->users.end()){
              channel->users.remove(*it);
              std::cout << user.first << " leaves channel " << channel->name << std::endl;
              if (channel->users.size() == 0) {
                kChannels.erase(channel->name);
                std::cout << "server: removing empty channel " << channel->name << std::endl;
              }
            }
            for (auto u : channel->users) {
              std::cout << "user: " << u->name << std::endl;
            }
            break;
          } else {
            std::cout << "server: " << user.first << " trying to leave non-existent channel " << channel->name << std::endl;
          }

        }
      }
      break;
    case REQ_JOIN:
      struct request_join join_request;
      memcpy(&join_request, buffer, sizeof(struct request_join));
      is_new_channel = true;

      // If channel does exists in global map, set local channel to channel from kChannels
      for (auto ch : kChannels) {
        if (join_request.req_channel == ch.second->name) {
          is_new_channel = false;
          channel = ch.second;
          break;
        }
      }

      // If channel is new create a new channel
      if (is_new_channel) {
        channel = new Channel(join_request.req_channel);
      }

      for (auto user : kUsers) {
        unsigned short current_port = user.second->port;
        in_addr_t current_address = user.second->address;

        if (current_port == user_port && current_address == user_address) {
          std::cout << "server: " << user.first << " joins channel "<< channel->name << std::endl;

          channel->users.push_back(user.second);

          // Otherwise
          if (is_new_channel) {
            kChannels.insert({channel->name, channel});
          }

          // Test print
          for (auto u : channel->users) {
            std::cout << "user: " << u->name << std::endl;
          }
          break;
        }

      }
      break;
    case REQ_SAY:
      struct request_say say_request;
      memcpy(&say_request, buffer, sizeof(struct request_say));

      std::cout << "user said: " << say_request.req_text << std::endl;
      for(auto user : kChannels[say_request.req_channel]->users){
        std::cout << "server sending message to: " << user->name << std::endl;
        struct sockaddr_in client_addr;
        struct text_say say;
        memcpy(&say, buffer, sizeof(struct text_say));

        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(user->port);
        client_addr.sin_addr.s_addr = htonl(user->address);

        strncpy(say.txt_channel, say_request.req_channel, CHANNEL_MAX);
        strncpy(say.txt_text, say_request.req_text, SAY_MAX);
        say.txt_type = TXT_SAY;
        strncpy(say.txt_username, user->name.c_str(), USERNAME_MAX);

        size_t message_size = sizeof(struct text_say);

        if (sendto(server_socket, &say, message_size, 0, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
          Error("server: failed to send say\n");
        }

      }
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
//  std::string domain;

  if (argc < 3) {
    std::cerr << "Usage: ./server domain_name port_num" << std::endl;
    exit(1);
  }

//  domain = argv[1];
  port = atoi(argv[2]);

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
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
//    std::cout << "before recvfrom" << std::endl;
    receive_len = recvfrom(server_socket, buffer, kBufferSize, 0, (struct sockaddr *) &client_addr, &client_addr_len);
    memcpy(&most_recent_client_addr, &client_addr, sizeof(most_recent_client_addr));
    std::cout << "port " << most_recent_client_addr.sin_port << std::endl;

    if (receive_len > 0) {
      buffer[receive_len] = 0;

//      std::cout << "port " << client_addr.sin_port << std::endl;

      ProcessRequest(server_socket, buffer, client_addr.sin_addr.s_addr, client_addr.sin_port);
    }
  }
}
