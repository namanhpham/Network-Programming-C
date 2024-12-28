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

// Luu yêu cầu kết bạn vào cơ sở dữ liệu 
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

int decline_friend_request(PGconn *conn, const char *from_username, const char *to_username)
{
    const char *paramValues[2] = {from_username, to_username};
    PGresult *res = PQexecParams(conn,
                                 "DELETE FROM friendship "
                                 "WHERE user_id = (SELECT id FROM users WHERE name = $1) "
                                 "AND friend_requested_user_id = (SELECT id FROM users WHERE name = $2)",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Decline friend request failed: %s", PQerrorMessage(conn));
        return 0;
    }

    PQclear(res);
    return 1;
}

int accept_friend_request(PGconn *conn, const char *from_username, const char *to_username)
{
    const char *paramValues[2] = {from_username, to_username};
    PGresult *res = PQexecParams(conn,
                                 "UPDATE friendship SET accepted_at = CURRENT_TIMESTAMP "
                                 "WHERE user_id = (SELECT id FROM users WHERE name = $1) "
                                 "AND friend_requested_user_id = (SELECT id FROM users WHERE name = $2)",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Accept friend request failed: %s", PQerrorMessage(conn));
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

// Xử lý yêu cầu chấp nhận kết bạn
void handle_accept_friend_request(Client *client, const char *payload, PGconn *conn)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    // Fetch the friend from the database
    PGresult *friend_res = get_user_by_username(conn, friend_username);
    if (friend_res == NULL || PQntuples(friend_res) == 0)
    {
        fprintf(stderr, "User %s does not exist.\n", friend_username);
        Message response = create_message(RESP_FAILURE, (uint8_t *)"User does not exist", 18);
        if (friend_res)
            PQclear(friend_res);
        return;
    }

    // Check if there is a pending friend request from this user
    const char *paramValues[2] = {friend_username, client->username};
    PGresult *res = PQexecParams(conn,
                                 "SELECT * FROM friendship WHERE user_id = (SELECT id FROM users WHERE name = $1) AND friend_requested_user_id = (SELECT id FROM users WHERE name = $2) AND accepted_at IS NULL",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */   

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Accept friend request failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return;
    }

    if (PQntuples(res) == 0)
    {
        // No pending friend request found
        Message response = create_message(RESP_FAILURE, (uint8_t *)"No pending friend request found", 30);
        send_message(client->socket, &response);
        PQclear(res);
        return;
    }

    // Accept the friend request
    if (accept_friend_request(conn, friend_username, client->username) == 0)
    {
        Message response = create_message(RESP_FAILURE, (uint8_t *)"Accept friend request failed", 26);
        send_message(client->socket, &response);
        PQclear(res);
        return;
    }

    // Notify the friend who sent the request
    Client *friend_client = get_online_client_by_username(friend_username); // Function to get client by username
    if (friend_client)
    {
        Message response = create_message(MSG_FRIEND_REQUEST_ACCEPTED, (uint8_t *)client->username, strlen(client->username));
        send_message(friend_client->socket, &response);
    }

    PQclear(res);
    PQclear(friend_res);
}

void handle_decline_friend_request(Client *client, const char *payload, PGconn *conn) {
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    // Fetch the friend from the database
    PGresult *friend_res = get_user_by_username(conn, friend_username);
    if (friend_res == NULL || PQntuples(friend_res) == 0)
    {
        fprintf(stderr, "User %s does not exist.\n", friend_username);
        Message response = create_message(RESP_FAILURE, (uint8_t *)"User does not exist", 18);
        if (friend_res)
            PQclear(friend_res);
        return;
    }

    // Check if there is a pending friend request from this user
    const char *paramValues[2] = {friend_username, client->username};
    PGresult *res = PQexecParams(conn,
                                 "SELECT * FROM friendship WHERE user_id = (SELECT id FROM users WHERE name = $1) AND friend_requested_user_id = (SELECT id FROM users WHERE name = $2) AND accepted_at IS NULL",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */   

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Decline friend request failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return;
    }

    if (PQntuples(res) == 0)
    {
        // No pending friend request found
        Message response = create_message(RESP_FAILURE, (uint8_t *)"No pending friend request found", 30);
        send_message(client->socket, &response);
        PQclear(res);
        return;
    }

    // Decline the friend request
    if (decline_friend_request(conn, friend_username, client->username) == 0)
    {
        Message response = create_message(RESP_FAILURE, (uint8_t *)"Decline friend request failed", 27);
        send_message(client->socket, &response);
        PQclear(res);
        return;
    }

    // Notify the friend who sent the request
    Client *friend_client = get_online_client_by_username(friend_username); // Function to get client by username
    if (friend_client)
    {
        Message response = create_message(MSG_FRIEND_REQUEST_DECLINED, (uint8_t *)client->username, strlen(client->username));
        send_message(friend_client->socket, &response);
    }

    PQclear(res);
    PQclear(friend_res);
}

void handle_see_friend_request(int client_socket, PGconn *conn)
{
    Client *client = get_client_by_socket(client_socket);
    if (client)
    {
        const char *paramValues[1] = {client->user_id};
        PGresult *res = PQexecParams(conn,
                                     "SELECT u.name FROM users u, friendship f WHERE f.user_id = u.id AND f.friend_requested_user_id = $1 AND f.accepted_at IS NULL",
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
