// CIS 432 Into to Networks
// Programming Project 1
// Fall 2015
//
// Benjamin Barnes
// H. Keith Hamm


#ifndef CIS432_PROJECT_1_SERVER_H
#define CIS432_PROJECT_1_SERVER_H

size_t kBufferSize = 2048;

class Channel;
class User;

void Error(std::string message);
void CreateSocket(char *domain, const char *port);
void HandleError(int server_socket, std::string channel, std::string type, in_addr_t request_address, unsigned short request_port);
void HandleLoginRequest(void *buffer, in_addr_t request_address, unsigned short request_port);
void HandleLogoutRequest(void *buffer, in_addr_t request_address, unsigned short request_port);
void HandleJoinRequest(void *buffer, in_addr_t request_address, unsigned short request_port);
void HandleLeaveRequest(int server_socket, void *buffer, in_addr_t request_address, unsigned short request_port);
void HandleSayRequest(int server_socket, void *buffer, in_addr_t request_address, unsigned short request_port);
void HandleListRequest(int server_socket, in_addr_t request_address, unsigned short request_port);
void HandleWhoRequest(int server_socket, void *buffer, in_addr_t request_address, unsigned short request_port);
void ProcessRequest(int server_socket, void *buffer, in_addr_t request_address, unsigned short request_port);

#endif //CIS432_PROJECT_1_SERVER_H
