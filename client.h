#ifndef CIS432_PROJECT_1_CLIENT2_H
#define CIS432_PROJECT_1_CLIENT2_H

#include <string>
#include <iostream>
#include <cstring>
#include <vector>
#include <sstream>
#include <iterator>
#include <netinet/in.h>

int kBufferSize = 2048;
int kClientPort = 5001;

void Error(const char *);
void Connect(char *, int);
std::vector<std::string> SplitString(std::string, char);
std::vector<std::string> StringSplit(std::string);
bool ProcessInput(std::string);
int SendLogin(struct request_login);

#endif //CIS432_PROJECT_1_CLIENT2_H
