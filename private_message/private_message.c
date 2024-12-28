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

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (online_clients[i] && strcmp(online_clients[i]->username, receiver_username) == 0)
        {
            Message msg = create_message(MSG_PRIVATE_MSG, (uint8_t *)message_content, strlen(message_content));
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
    PQclear(res);
    PQclear(user_res);
    PQclear(receiver_res);
}