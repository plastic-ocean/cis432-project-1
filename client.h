// CIS 432 Into to Networks
// Programming Project 1
// Fall 2015
//
// Benjamin Barnes
// H. Keith Hamm


#ifndef CIS432_PROJECT_1_CLIENT2_H
#define CIS432_PROJECT_1_CLIENT2_H

#include <string>
#include <iostream>
#include <cstring>
#include <vector>
#include <sstream>
#include <iterator>
#include <netinet/in.h>

size_t kBufferSize = 2048;

void Error(const char *msg);
void PrintPrompt();
void ClearPrompt();
bool ProcessInput(std::string);
std::vector<std::string> SplitString(std::string input);
void CreateSocket(char *domain, const char *port);
int SendSay(std::string message);
int SendLogin(char *username);
int SendLogout();
int SendList();
int SendWho(std::string channel);
int SendLeave(std::string channel);
int SwitchChannel(std::string channel);
int SendJoin(std::string channel);
void HandleTextSay(char *receive_buffer, char *output);
void HandleTextList(char *receive_buffer, char *output);
void HandleTextWho(char *receive_buffer, char *output);
void HandleError(char *receive_buffer, char *output);
bool ProcessInput(std::string input);



#endif //CIS432_PROJECT_1_CLIENT2_H
