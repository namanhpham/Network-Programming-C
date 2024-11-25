// friendship.h
#ifndef FRIENDSHIP_H
#define FRIENDSHIP_H

#include "protocol.c"  // Giả sử file protocol.c chứa định nghĩa của struct Message và các mã sự kiện
#include <stdint.h>

typedef struct {
    char username1[128];
    char username2[128];
} FriendPair;

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

#endif
