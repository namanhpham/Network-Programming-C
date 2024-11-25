#ifndef USER_MANAGEMENT_H
#define USER_MANAGEMENT_H

void handle_register(int client_socket, const char *payload);
void handle_login(int client_socket, const char *payload);
void handle_logout(int client_socket);

#endif // USER_MANAGEMENT_H
