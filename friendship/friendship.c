// friendship.c
#include <string.h>
#include "friendship.h"
#include "../protocol.h" // File này sẽ chứa hàm send_message, create_message, v.v. nếu cần
#include "../common.h"
#include "../utils.h"
#include <stdio.h>

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

void handle_send_friend_request(Client *client, const char *payload, PGconn *conn)
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
        log_file("Friend request sent from %s to %s\n", client->username, friend_username);
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
        send_message(client->socket, &response);
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
        log_file("Friend request accepted from %s to %s\n", client->username, friend_username);
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
        send_message(client->socket, &response);
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
        log_file("Friend request declined from %s to %s\n", client->username, friend_username);
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
        log_file("Sent friend requests list to %s\n", client->username);
        PQclear(res);
    }
}

// Check if two users are friends
int are_friends(PGconn *conn, const char *username1, const char *username2)
{
    const char *paramValues[2] = {username1, username2};
    PGresult *res = PQexecParams(conn,
                                 "SELECT * FROM friendship WHERE (user_id = (SELECT id FROM users WHERE name = $1) AND friend_requested_user_id = (SELECT id FROM users WHERE name = $2) AND accepted_at IS NOT NULL) OR (user_id = (SELECT id FROM users WHERE name = $2) AND friend_requested_user_id = (SELECT id FROM users WHERE name = $1) AND accepted_at IS NOT NULL)",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Check friendship status failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }

    int result = PQntuples(res) > 0;
    PQclear(res);
    return result;
}

// Lấy danh sách bạn bè
void handle_see_friend_list(int client_socket, PGconn *conn)
{
    Client *client = get_client_by_socket(client_socket);
    if (client)
    {
        const char *paramValues[1] = {client->user_id};
        PGresult *res = PQexecParams(conn,
                                     "SELECT u.name FROM users u, friendship f WHERE (f.user_id = u.id AND f.friend_requested_user_id = $1 AND f.accepted_at IS NOT NULL) OR (f.friend_requested_user_id = u.id AND f.user_id = $1 AND f.accepted_at IS NOT NULL)",
                                     1,       /* one param */
                                     NULL,    /* let the backend deduce param type */
                                     paramValues,
                                     NULL,    /* don't need param lengths since text */
                                     NULL,    /* default to all text params */
                                     0);      /* ask for binary results */

        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
            fprintf(stderr, "Fetch friend list failed: %s", PQerrorMessage(conn));
            PQclear(res);
            return;
        }

        char friend_list[1024] = {0};
        char friend_numbers[256];
        sprintf(friend_numbers, "Friend numbers: %d\n", PQntuples(res));
        strcat(friend_list, friend_numbers);
        for (int i = 0; i < PQntuples(res); i++)
        {
            char *friend_username = PQgetvalue(res, i, 0);
            strcat(friend_list, friend_username);
            strcat(friend_list, "\n");
        }

        Message friend_list_msg = create_message(MSG_FRIENDS_LIST, (uint8_t *)friend_list, strlen(friend_list));
        send_message(client_socket, &friend_list_msg);
        printf("Sent friend list to %s\n", client->username);
        log_file("Sent friend list to %s\n", client->username);
        PQclear(res);
    }
}

// Handle remove friend request
void handle_remove_friend(Client *client, const char *payload, PGconn *conn)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    // Check if the users are friends
    if (!are_friends(conn, client->username, friend_username))
    {
        Message response = create_message(RESP_FAILURE, (uint8_t *)"You are not friends with this user", 33);
        send_message(client->socket, &response);
        return;
    }

    // Remove the friendship
    const char *paramValues[2] = {client->username, friend_username};
    PGresult *res = PQexecParams(conn,
                                 "DELETE FROM friendship WHERE (user_id = (SELECT id FROM users WHERE name = $1) AND friend_requested_user_id = (SELECT id FROM users WHERE name = $2)) OR (user_id = (SELECT id FROM users WHERE name = $2) AND friend_requested_user_id = (SELECT id FROM users WHERE name = $1))",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Remove friend failed: %s", PQerrorMessage(conn));
        PQclear(res);
        Message response = create_message(RESP_FAILURE, (uint8_t *)"Remove friend failed", 20);
        send_message(client->socket, &response);
        return;
    }

    PQclear(res);

    // Notify both users about the removal
    Client *friend_client = get_online_client_by_username(friend_username);
    if (friend_client)
    {
        Message response = create_message(MSG_FRIEND_REMOVED, (uint8_t *)client->username, strlen(client->username));
        send_message(friend_client->socket, &response);
        log_file("%s removed %s from friends\n", client->username, friend_username);
    }

    Message response = create_message(RESP_SUCCESS, (uint8_t *)"Friend removed successfully", 27);
    send_message(client->socket, &response);
}
