#include <string.h>
#include <stdio.h>
#include "user_management.h"
#include <netinet/in.h>
#include "../protocol.h" // Assuming protocol handling is also separated
#include "../common.h"

// Hàm xử lý yêu cầu đăng ký từ client
void handle_register(int client_socket, const char *payload, PGconn *conn)
{
    char username[128], password[128];
    sscanf(payload, "%127[^:]:%127s", username, password); // Tách username và password

    if (username_exists(conn, username))
    {
        Message msg = create_message(RESP_FAILURE, (uint8_t *)"Username already exists", 23);
        send_message(client_socket, &msg);
        return;
    }

    if (save_account(conn, username, password))
    {
        Message msg = create_message(RESP_SUCCESS, (uint8_t *)"Registration successful", 23);
        send_message(client_socket, &msg);
    }
    else
    {
        Message msg = create_message(RESP_FAILURE, (uint8_t *)"Registration failed", 19);
        send_message(client_socket, &msg);
    }
}

void handle_login(int client_socket, const char *payload, PGconn *conn)
{
    char username[128], password[128];
    sscanf(payload, "%127[^:]:%127s", username, password); // Split username and password

    if (validate_credentials(conn, username, password))
    {
        Client *existing_client = get_online_client_by_username(username);
        if (existing_client)
        {
            Message msg = create_message(RESP_FAILURE, (uint8_t *)"User already logged in", 22);
            send_message(client_socket, &msg);
            return;
        }

        Client *client = get_client_by_socket(client_socket);
        if (client)
        {
            strcpy(client->username, username);
            client->is_logged_in = 1;
            add_online_client(client);

            // Update last_online_at
            const char *paramValues[1] = {username};
            PGresult *res = PQexecParams(conn,
                                         "UPDATE users SET last_online_at = CURRENT_TIMESTAMP WHERE name = $1",
                                         1,       /* one param */
                                         NULL,    /* let the backend deduce param type */
                                         paramValues,
                                         NULL,    /* don't need param lengths since text */
                                         NULL,    /* default to all text params */
                                         0);      /* ask for binary results */
            PQclear(res);

            Message msg = create_message(RESP_SUCCESS, (uint8_t *)"Login successful", 16);
            send_message(client_socket, &msg);

            // Notify all clients of the updated online user list
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (online_clients[i] && online_clients[i]->is_logged_in)
                {
                    send_online_users_list(online_clients[i]->socket);
                }
            }
        }
    }
    else
    {
        Message msg = create_message(RESP_FAILURE, (uint8_t *)"Invalid credentials", 19);
        send_message(client_socket, &msg);
    }
}

void handle_logout(int client_socket, PGconn *conn)
{
    Client *client = get_client_by_socket(client_socket);
    if (client && client->is_logged_in)
    {
        // Update last_offline_at
        const char *paramValues[1] = {client->username};
        PGresult *res = PQexecParams(conn,
                                     "UPDATE users SET last_offline_at = CURRENT_TIMESTAMP WHERE name = $1",
                                     1,       /* one param */
                                     NULL,    /* let the backend deduce param type */
                                     paramValues,
                                     NULL,    /* don't need param lengths since text */
                                     NULL,    /* default to all text params */
                                     0);      /* ask for binary results */
        PQclear(res);

        client->is_logged_in = 0;
        remove_online_client(client);

        // Notify all clients of the updated online user list
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (online_clients[i] && online_clients[i]->is_logged_in)
            {
                send_online_users_list(online_clients[i]->socket);
            }
        }

        // Send logout success message to the client
        Message msg = create_message(MSG_LOGOUT, (uint8_t *)"Logout successful", 17);
        send_message(client_socket, &msg);
    }
}

void handle_disconnect(int client_socket, PGconn *conn)
{
    Client *client = get_client_by_socket(client_socket);
    if (client && client->is_logged_in)
    {
        // Update last_offline_at
        const char *paramValues[1] = {client->username};
        PGresult *res = PQexecParams(conn,
                                     "UPDATE users SET last_offline_at = CURRENT_TIMESTAMP WHERE name = $1",
                                     1,       /* one param */
                                     NULL,    /* let the backend deduce param type */
                                     paramValues,
                                     NULL,    /* don't need param lengths since text */
                                     NULL,    /* default to all text params */
                                     0);      /* ask for binary results */
        PQclear(res);

        client->is_logged_in = 0;
        remove_online_client(client);

        // Notify all clients of the updated online user list
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (online_clients[i] && online_clients[i]->is_logged_in)
            {
                send_online_users_list(online_clients[i]->socket);
            }
        }
    }
}
