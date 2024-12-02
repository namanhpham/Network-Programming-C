#include "common.h"
#include <stdio.h>
#include <string.h>

Client *clients[MAX_CLIENTS] = {NULL};          // Global array of clients
Client *online_clients[MAX_CLIENTS] = {NULL};  // Global array of online clients

Client *get_client_by_socket(int client_socket)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] && clients[i]->socket == client_socket)
        {
            return clients[i]; // Return the Client pointer directly
        }
    }
    return NULL;
}

Client *get_online_client_by_username(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (online_clients[i] && strcmp(online_clients[i]->username, username) == 0)
        {
            return online_clients[i];
        }
    }
    return NULL;
}

int username_exists(PGconn *conn, const char *username)
{
    const char *paramValues[1] = {username};
    PGresult *res = PQexecParams(conn,
                                 "SELECT 1 FROM users WHERE name = $1",
                                 1,       /* one param */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Username existence check failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }

    int exists = PQntuples(res) > 0;
    PQclear(res);
    return exists;
}

int save_account(PGconn *conn, const char *username, const char *password)
{
    const char *paramValues[2] = {username, password};
    PGresult *res = PQexecParams(conn,
                                 "INSERT INTO users (name, password) VALUES ($1, $2)",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Save account failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }

    PQclear(res);
    return 1;
}

void trim_newline(char *str)
{
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[len - 1] = '\0';
    }
}

int validate_credentials(PGconn *conn, const char *username, const char *password)
{
    const char *paramValues[2] = {username, password};
    PGresult *res = PQexecParams(conn,
                                 "SELECT 1 FROM users WHERE name = $1 AND password = $2",
                                 2,       /* two params */
                                 NULL,    /* let the backend deduce param type */
                                 paramValues,
                                 NULL,    /* don't need param lengths since text */
                                 NULL,    /* default to all text params */
                                 0);      /* ask for binary results */

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Validate credentials failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }

    int valid = PQntuples(res) > 0;
    PQclear(res);
    return valid;
}

void add_online_client(Client *client)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (online_clients[i] == NULL)
        {
            online_clients[i] = client;
            break;
        }
    }
}

void send_online_users_list(int client_socket)
{
    char online_users[1024] = {0};

    // Create a comma-separated list of online usernames
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (online_clients[i] && online_clients[i]->is_logged_in)
        {
            strcat(online_users, online_clients[i]->username);
            strcat(online_users, ",");
        }
    }

    // Remove the trailing comma, if any
    size_t len = strlen(online_users);
    if (len > 0)
    {
        online_users[len - 1] = '\0';
    }

    // Send the list to the specified client
    Message msg = create_message(MSG_ONLINE_USERS, (uint8_t *)online_users, strlen(online_users));
    send_message(client_socket, &msg);
}