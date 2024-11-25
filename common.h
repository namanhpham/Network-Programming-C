#ifndef COMMON_H
#define COMMON_H

#include "protocol.h" // Include nếu kiểu Client được định nghĩa ở đây, hoặc định nghĩa Client nếu chưa có.

#define MAX_CLIENTS 10
#define ACCOUNTS_FILE "accounts.txt"

// Khai báo các mảng clients.
extern Client *clients[MAX_CLIENTS];
extern Client *online_clients[MAX_CLIENTS];

// Function prototypes
Client *get_client_by_socket(int client_socket);
Client *get_online_client_by_username(const char *username);
int username_exists(const char *username);
int save_account(const char *username, const char *password);
int validate_credentials(const char *username, const char *password);
void add_online_client(Client *client);
void send_online_users_list(int client_socket);

#endif // COMMON_H
