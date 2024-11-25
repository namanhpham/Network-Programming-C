// friendship.c
#include <string.h>
#include "friendship.h"
#include "../protocol.h" // File này sẽ chứa hàm send_message, create_message, v.v. nếu cần
#include "../protocol.c"
#include "../common.h"

FriendPair friends[MAX_FRIENDS] = {0};

int is_online(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        // Kiểm tra nếu client ở vị trí i có username khớp và đang kết nối
        if (clients[i]->socket > 0 && strcmp(clients[i]->username, username) == 0)
        {
            return 1; // Người dùng đang trực tuyến
        }
    }
    return 0; // Người dùng không trực tuyến
}

// Thêm bạn vào danh sách bạn bè
void add_friend(const char *username1, const char *username2)
{
    for (int i = 0; i < MAX_FRIENDS; i++)
    {
        if (strlen(friends[i].username1) == 0)
        {
            strcpy(friends[i].username1, username1);
            strcpy(friends[i].username2, username2);
            break;
        }
    }
}

// Kiểm tra xem hai người dùng có phải bạn bè không
int is_friend(const char *username1, const char *username2)
{
    for (int i = 0; i < MAX_FRIENDS; i++)
    {
        if ((strcmp(friends[i].username1, username1) == 0 && strcmp(friends[i].username2, username2) == 0) ||
            (strcmp(friends[i].username1, username2) == 0 && strcmp(friends[i].username2, username1) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// Xóa bạn khỏi danh sách bạn bè
void remove_friend(const char *username1, const char *username2)
{
    for (int i = 0; i < MAX_FRIENDS; i++)
    {
        if ((strcmp(friends[i].username1, username1) == 0 && strcmp(friends[i].username2, username2) == 0) ||
            (strcmp(friends[i].username1, username2) == 0 && strcmp(friends[i].username2, username1) == 0))
        {
            memset(&friends[i], 0, sizeof(FriendPair)); // Xóa bạn khỏi danh sách
            break;
        }
    }
}

// Xử lý yêu cầu chấp nhận kết bạn
void handle_accept_friend_request(int client_socket, const char *payload)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket); // Hàm giả định trả về client từ socket

    if (client && !is_friend(client->username, friend_username))
    {
        add_friend(client->username, friend_username);

        // Gửi thông báo cho người gửi lời mời
        Client *friend_client = get_online_client_by_username(friend_username); // Hàm trả về client theo username
        if (friend_client)
        {
            Message response = create_message(MSG_FRIEND_REQUEST_ACCEPTED, (uint8_t *)client->username, strlen(client->username));
            send_message(friend_client->socket, &response);
        }
    }
}

// Xử lý yêu cầu từ chối kết bạn
void handle_decline_friend_request(int client_socket, const char *payload)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket);

    if (client)
    {
        Client *friend_client = get_online_client_by_username(friend_username);
        if (friend_client)
        {
            Message response = create_message(MSG_FRIEND_REQUEST_DECLINED, (uint8_t *)client->username, strlen(client->username));
            send_message(friend_client->socket, &response);
        }
    }
}

// Xử lý yêu cầu hủy kết bạn
void handle_remove_friend(int client_socket, const char *payload)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket);
    if (client && is_friend(client->username, friend_username))
    {
        remove_friend(client->username, friend_username);

        // Thông báo cho cả hai bên về việc hủy kết bạn
        Client *friend_client = get_online_client_by_username(friend_username);
        if (friend_client)
        {
            Message response = create_message(MSG_FRIEND_REMOVED, (uint8_t *)client->username, strlen(client->username));
            send_message(friend_client->socket, &response);
        }
    }
}

// Xử lý yêu cầu lấy danh sách bạn bè
void handle_get_friends_list(int client_socket)
{
    Client *client = get_client_by_socket(client_socket);
    if (client)
    {
        char friends_list[1024] = {0};

        for (int i = 0; i < MAX_FRIENDS; i++)
        {
            if (is_friend(client->username, friends[i].username1))
            {
                strcat(friends_list, friends[i].username1);
            }
            else if (is_friend(client->username, friends[i].username2))
            {
                strcat(friends_list, friends[i].username2);
            }

            // Thêm trạng thái
            if (is_online(friends[i].username1) || is_online(friends[i].username2))
            {
                strcat(friends_list, ":online,");
            }
            else
            {
                strcat(friends_list, ":offline,");
            }
        }

        Message response = create_message(MSG_FRIENDS_LIST, (uint8_t *)friends_list, strlen(friends_list));
        send_message(client_socket, &response);
    }
}
