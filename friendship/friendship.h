// friendship.h
#ifndef FRIENDSHIP_H
#define FRIENDSHIP_H

#include "../protocol.h" // File này sẽ chứa hàm send_message, create_message, v.v. nếu cần
#include <stdint.h>

#define MAX_FRIENDS 100  // Định nghĩa số lượng bạn bè tối đa (có thể điều chỉnh)

extern FriendPair friends[MAX_FRIENDS];

// Khai báo các hàm liên quan đến quản lý bạn bè
void add_friend(const char *username1, const char *username2);
int is_friend(const char *username1, const char *username2);
void remove_friend(const char *username1, const char *username2);
void handle_accept_friend_request(int client_socket, const char *payload);
void handle_decline_friend_request(int client_socket, const char *payload);
void handle_remove_friend(int client_socket, const char *payload);
void handle_get_friends_list(int client_socket);
int is_online(const char *username);
void handle_friend_request(Client *client, const char *payload);
void handle_see_friend_request(int client_socket);
void save_friend_request(const char *from_username, const char *to_username);

#endif
