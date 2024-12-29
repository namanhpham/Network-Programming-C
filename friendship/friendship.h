// friendship.h
#ifndef FRIENDSHIP_H
#define FRIENDSHIP_H

#include "../protocol.h" // File này sẽ chứa hàm send_message, create_message, v.v. nếu cần
#include <stdint.h>
#include <libpq-fe.h> // Include libpq-fe.h for PGconn

#define MAX_FRIENDS 100  // Định nghĩa số lượng bạn bè tối đa (có thể điều chỉnh)

extern FriendPair friends[MAX_FRIENDS];

// Khai báo các hàm liên quan đến quản lý bạn bè
void handle_accept_friend_request(Client *client, const char *payload, PGconn *conn);
void handle_decline_friend_request(Client *client, const char *payload, PGconn *conn);
int is_online(const char *username);
void handle_send_friend_request(Client *client, const char *payload, PGconn *conn);
void handle_see_friend_request(int client_socket, PGconn *conn);
int save_friend_request(PGconn *conn, const char *from_username, const char *to_username);
void handle_see_friend_list(int client_socket, PGconn *conn);
void handle_remove_friend(Client *client, const char *payload, PGconn *conn);

#endif
