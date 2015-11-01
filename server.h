#ifndef CIS432_PROJECT_1_SERVER_H
#define CIS432_PROJECT_1_SERVER_H

int kBufferSize = 2048;

class Channel;
class User;

void Error(const char *message);
void ProcessRequest(void *buffer);

#endif //CIS432_PROJECT_1_SERVER_H
