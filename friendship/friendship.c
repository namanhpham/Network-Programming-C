// friendship.c
#include <string.h>
#include "friendship.h"
#include "../protocol.h" // File này sẽ chứa hàm send_message, create_message, v.v. nếu cần
#include "../common.h"
#include "../utils.h"
#include <stdio.h>

#define FRIEND_REQUEST_FILE "friend_requests.txt"

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
void add_friend(PGconn *conn, const char *username1, const char *username2)
{
    const char *paramValues[2] = {username1, username2};
    PGresult *res = PQexecParams(conn,
                                 "INSERT INTO friendship (user_id, friend_requested_user_id, accepted_at) VALUES ($1, $2, CURRENT_TIMESTAMP)",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Add friend failed: %s", PQerrorMessage(conn));
    }

    PQclear(res);
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
void handle_accept_friend_request(int client_socket, const char *payload, PGconn *conn)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    // Fetch the client from the database
    PGresult *client_res = get_user_by_username(conn, friend_username);
    if (client_res == NULL || PQntuples(client_res) == 0)
    {
        fprintf(stderr, "User %s does not exist.\n", friend_username);
        if (client_res)
            PQclear(client_res);
        return;
    }

    char *client_username = PQgetvalue(client_res, 0, 1); // Assuming the username is in the second column

    if (!is_friend(client_username, friend_username))
    {
        add_friend(conn, client_username, friend_username);

        // Gửi thông báo cho người gửi lời mời
        Client *friend_client = get_online_client_by_username(friend_username); // Hàm trả về client theo username
        if (friend_client)
        {
            Message response = create_message(MSG_FRIEND_REQUEST_ACCEPTED, (uint8_t *)client_username, strlen(client_username));
            send_message(friend_client->socket, &response);
        }
    }

    PQclear(client_res);
}

// Xử lý yêu cầu từ chối kết bạn
void handle_decline_friend_request(int client_socket, const char *payload, PGconn *conn)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    // Fetch the client from the database
    PGresult *client_res = get_user_by_username(conn, friend_username);
    if (client_res == NULL || PQntuples(client_res) == 0)
    {
        fprintf(stderr, "User %s does not exist.\n", friend_username);
        if (client_res)
            PQclear(client_res);
        return;
    }

    char *client_username = PQgetvalue(client_res, 0, 1); // Assuming the username is in the second column

    Client *friend_client = get_online_client_by_username(friend_username);
    if (friend_client)
    {
        Message response = create_message(MSG_FRIEND_REQUEST_DECLINED, (uint8_t *)client_username, strlen(client_username));
        send_message(friend_client->socket, &response);
    }

    PQclear(client_res);
}

// Xử lý yêu cầu hủy kết bạn
void handle_remove_friend(int client_socket, const char *payload, PGconn *conn)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket);
    if (client && is_friend(client->username, friend_username))
    {
        const char *paramValues[2] = {client->username, friend_username};
        PGresult *res = PQexecParams(conn,
                                     "DELETE FROM friendship WHERE (user_id = $1 AND friend_requested_user_id = $2) OR (user_id = $2 AND friend_requested_user_id = $1)",
                                     2,       /* two params */
                                     NULL,    /* let the backend deduce param type */
                                     paramValues,
                                     NULL,    /* don't need param lengths since text */
                                     NULL,    /* default to all text params */
                                     0);      /* ask for binary results */

        if (PQresultStatus(res) != PGRES_COMMAND_OK)
        {
            fprintf(stderr, "Remove friend failed: %s", PQerrorMessage(conn));
        }

        PQclear(res);

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
void handle_get_friends_list(int client_socket, PGconn *conn)
{
    Client *client = get_client_by_socket(client_socket);
    if (client)
    {
        const char *paramValues[1] = {client->username};
        PGresult *res = PQexecParams(conn,
                                     "SELECT user_id, friend_requested_user_id FROM friendship WHERE user_id = $1 AND accepted_at IS NOT NULL",
                                     1,       /* one param */
                                     NULL,    /* let the backend deduce param type */
                                     paramValues,
                                     NULL,    /* don't need param lengths since text */
                                     NULL,    /* default to all text params */
                                     0);      /* ask for binary results */

        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
            fprintf(stderr, "Fetch friends list failed: %s", PQerrorMessage(conn));
            PQclear(res);
            return;
        }

        char friends_list[1024] = {0};
        for (int i = 0; i < PQntuples(res); i++)
        {
            char *user_id = PQgetvalue(res, i, 0);
            char *friend_requested_user_id = PQgetvalue(res, i, 1);

            if (strcmp(user_id, client->username) == 0)
            {
                strcat(friends_list, friend_requested_user_id);
            }
            else
            {
                strcat(friends_list, user_id);
            }

            // Thêm trạng thái
            if (is_online(user_id) || is_online(friend_requested_user_id))
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

        PQclear(res);
    }
}

int save_friend_request(PGconn *conn, const char *from_username, const char *to_username)
{
    const char *paramValues[2] = {from_username, to_username};
    PGresult *res = PQexecParams(conn,
                                 "INSERT INTO friendship (user_id, friend_requested_user_id) "
                                 "SELECT u1.id, u2.id FROM users u1, users u2 "
                                 "WHERE u1.name = $1 AND u2.name = $2",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Save friend request failed: %s", PQerrorMessage(conn));
        return 0;
    }

    PQclear(res);
    return 1;
}

void handle_friend_request(Client *client, const char *payload, PGconn *conn)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    const char *paramValues[1] = {friend_username};
    PGresult *res = PQexecParams(conn,
                                 "SELECT id FROM users WHERE name = $1",
                                 1,       /* one param */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Fetch user failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return;
    }

    if (PQntuples(res) > 0)
    {
        if(save_friend_request(conn, client->username, friend_username) == 0)
        {
            Message response = create_message(RESP_FAILURE, (uint8_t *)"Friend request failed", 21);
            send_message(client->socket, &response);
            PQclear(res);
            return;
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (online_clients[i] && strcmp(online_clients[i]->username, friend_username) == 0)
            {
                Message friend_request_msg = create_message(MSG_FRIEND_REQUEST, (uint8_t *)client->username, strlen(client->username));
                send_message(online_clients[i]->socket, &friend_request_msg);
                Message response = create_message(RESP_SUCCESS, (uint8_t *)"Friend request sent", 19);
                send_message(client->socket, &response);
                break;
            }
        }
        printf("Friend request sent from %s to %s\n", client->username, friend_username);
    }
    else
    {
        printf("User %s not found\n", friend_username);
    }

    PQclear(res);
}

void handle_see_friend_request(int client_socket, PGconn *conn)
{
    Client *client = get_client_by_socket(client_socket);
    if (client)
    {
        const char *paramValues[1] = {client->user_id};
        PGresult *res = PQexecParams(conn,
                                     "SELECT u.name FROM users u, friendship f WHERE f.friend_requested_user_id = u.id AND f.user_id = $1 AND f.accepted_at IS NULL",
                                     1,       /* one param */
                                     NULL,    /* let the backend deduce param type */
                                     paramValues,
                                     NULL,    /* don't need param lengths since text */
                                     NULL,    /* default to all text params */
                                     0);      /* ask for binary results */

        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
            fprintf(stderr, "Fetch friend requests failed: %s", PQerrorMessage(conn));
            PQclear(res);
            return;
        }

        char friend_requests_list[1024] = {0};
        char request_numbers[256];
        sprintf(request_numbers, "Friend requests number: %d\n", PQntuples(res));
        strcat(friend_requests_list, request_numbers);
        for (int i = 0; i < PQntuples(res); i++)
        {
            char *from_username = PQgetvalue(res, i, 0);
            strcat(friend_requests_list, from_username);
            strcat(friend_requests_list, "\n");
        }

        Message friend_request_msg = create_message(MSG_FRIEND_REQUEST_LIST, (uint8_t *)friend_requests_list, strlen(friend_requests_list));
        send_message(client_socket, &friend_request_msg);
        printf("Sent friend requests list to %s\n", client->username);

        PQclear(res);
    }
}
