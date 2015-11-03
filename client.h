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

void Error(const char *);
std::vector<std::string> SplitString(std::string);
bool ProcessInput(std::string);

#endif //CIS432_PROJECT_1_CLIENT2_H
