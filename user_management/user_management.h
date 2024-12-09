#ifndef USER_MANAGEMENT_H
#define USER_MANAGEMENT_H

#include "../protocol.h"
#include <libpq-fe.h>

void handle_register(int client_socket, const char *payload, PGconn *conn);
void handle_login(int client_socket, const char *payload, PGconn *conn);
void handle_logout(int client_socket, PGconn *conn);
void handle_disconnect(int client_socket, PGconn *conn);
#endif // USER_MANAGEMENT_H

