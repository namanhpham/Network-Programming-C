#include "private_message.h"
#include <string.h> // Include string.h for strcmp and strlen
#include <libpq-fe.h>
#include "../protocol.h"
#include "../common.h"

void handle_private_message(Client *client, char *payload, PGconn *conn)
{
    char receiver_username[128], message_content[512];
    sscanf(payload, "%127[^:]:%511[^\n]", receiver_username, message_content);

    // Find user_id and receiver_id
    const char *user_query = "SELECT id FROM users WHERE name = $1";
    const char *paramValuesUser[1] = {client->username};
    PGresult *user_res = PQexecParams(conn, user_query, 1, NULL, paramValuesUser, NULL, NULL, 0);

    if (PQresultStatus(user_res) != PGRES_TUPLES_OK || PQntuples(user_res) == 0)
    {
        fprintf(stderr, "User not found: %s", PQerrorMessage(conn));
        PQclear(user_res);
        return;
    }
    char *user_id = PQgetvalue(user_res, 0, 0);

    const char *receiver_query = "SELECT id FROM users WHERE name = $1";
    const char *paramValuesReceiver[1] = {receiver_username};
    PGresult *receiver_res = PQexecParams(conn, receiver_query, 1, NULL, paramValuesReceiver, NULL, NULL, 0);

    if (PQresultStatus(receiver_res) != PGRES_TUPLES_OK || PQntuples(receiver_res) == 0)
    {
        fprintf(stderr, "Receiver not found: %s", PQerrorMessage(conn));
        PQclear(receiver_res);
        PQclear(user_res);
        return;
    }
    char *receiver_id = PQgetvalue(receiver_res, 0, 0);

    char full_message[768];
    snprintf(full_message, sizeof(full_message), "%s: ", client->username);
    strncat(full_message, message_content, sizeof(full_message) - strlen(full_message) - 1);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (online_clients[i] && strcmp(online_clients[i]->username, receiver_username) == 0)
        {
            Message msg = create_message(MSG_PRIVATE_MSG, (uint8_t *)full_message, strlen(full_message));
            send_message(online_clients[i]->socket, &msg);
            break;
        }
    }

    // Save message to database
    const char *paramValues[4] = {user_id, receiver_id, message_content, "NOW()"};
    PGresult *res = PQexecParams(conn,
                                 "INSERT INTO message (user_id, receiver_id, content, created_at) VALUES ($1, $2, $3, $4)",
                                 4,    /* four params */
                                 NULL, /* let the backend deduce param type */
                                 paramValues,
                                 NULL, /* don't need param lengths since text */
                                 NULL, /* default to all text params */
                                 0);   /* ask for binary results */

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Save message failed: %s", PQerrorMessage(conn));
    }
    log_file("User %s sent a private message to %s", client->username, receiver_username);
    PQclear(res);
    PQclear(user_res);
    PQclear(receiver_res);
}

void handle_see_private_messages(Client *client, char *payload, PGconn *conn)
{
    char receiver_username[128];
    sscanf(payload, "%127s", receiver_username);

    const char *user_query = "SELECT id FROM users WHERE name = $1";
    const char *paramValuesUser[1] = {client->username};
    PGresult *user_res = PQexecParams(conn, user_query, 1, NULL, paramValuesUser, NULL, NULL, 0);

    if (PQresultStatus(user_res) != PGRES_TUPLES_OK || PQntuples(user_res) == 0)
    {
        fprintf(stderr, "User not found: %s", PQerrorMessage(conn));
        PQclear(user_res);
        return;
    }
    char *user_id = PQgetvalue(user_res, 0, 0);

    const char *receiver_query = "SELECT id FROM users WHERE name = $1";
    const char *paramValuesReceiver[1] = {receiver_username};
    PGresult *receiver_res = PQexecParams(conn, receiver_query, 1, NULL, paramValuesReceiver, NULL, NULL, 0);

    if (PQresultStatus(receiver_res) != PGRES_TUPLES_OK || PQntuples(receiver_res) == 0)
    {
        fprintf(stderr, "Receiver not found: %s", PQerrorMessage(conn));
        PQclear(receiver_res);
        PQclear(user_res);
        return;
    }
    char *receiver_id = PQgetvalue(receiver_res, 0, 0);

    const char *message_query = "SELECT u.name, m.content FROM message m JOIN users u ON m.user_id = u.id WHERE (m.user_id = $1 AND m.receiver_id = $2) OR (m.user_id = $2 AND m.receiver_id = $1) ORDER BY m.created_at ASC";
    const char *paramValues[2] = {user_id, receiver_id};
    PGresult *message_res = PQexecParams(conn, message_query, 2, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(message_res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Failed to fetch messages: %s", PQerrorMessage(conn));
        PQclear(message_res);
        PQclear(receiver_res);
        PQclear(user_res);
        return;
    }

    char history[4096] = "";
    for (int i = 0; i < PQntuples(message_res); i++)
    {
        strcat(history, PQgetvalue(message_res, i, 0));
        strcat(history, ": ");
        strcat(history, PQgetvalue(message_res, i, 1));
        strcat(history, "\n");
    }

    Message msg = create_message(MSG_PRIVATE_MSG_HISTORY, (uint8_t *)history, strlen(history));
    send_message(client->socket, &msg);
    log_file("Sent private message history to %s", client->username);

    PQclear(message_res);
    PQclear(receiver_res);
    PQclear(user_res);
}