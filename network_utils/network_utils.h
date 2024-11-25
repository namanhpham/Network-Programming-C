#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <sys/socket.h>
#include <netinet/in.h>

int initialize_socket(int port);
void close_socket(int socket);

#endif
