#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "network_utils/network_utils.h"
#include "user_management/user_management.h"
#include "message_handling/message_handling.h"
#include "friendship/friendship.h"
#include "group/group.h" // Include the group header file
#include "private_message/private_message.h"
#include "protocol.h"
#include "common.h"
#include <libpq-fe.h>
#include "utils.h"

#define PORT 8080
#define MAX_FRIENDS 100 // Định nghĩa số lượng bạn bè tối đa (có thể điều chỉnh)
#define FRIEND_REQUEST_FILE "friend_requests.txt"
#define CONNECTION_STRING_LENGTH 512

// Function prototypes
void handle_see_private_messages(Client *client, char *payload, PGconn *conn);

// Hàm để thêm client vào danh sách
void add_client(Client *client)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] == NULL)
        {
            clients[i] = client;
            break;
        }
    }
}

// Hàm xóa client khỏi danh sách
void remove_client(int socket)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] && clients[i]->socket == socket)
        {
            clients[i] = NULL;
            break;
        }
    }
}
void *handle_client(void *arg)
{
    Client *client = (Client *)arg;
    Message message;
    ssize_t received;

    const char *db_url = getenv("DB_URL");
    PGconn *conn = PQconnectdb(db_url);
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        pthread_exit(NULL);
    }

    while ((received = receive_message(client->socket, &message)) > 0)
    {
        switch (message.type)
        {
        case MSG_REGISTER:
            handle_register(client->socket, (char *)message.payload, conn);
            break;
        case MSG_LOGIN:
            handle_login(client->socket, (char *)message.payload, conn);
            break;
        case MSG_PRIVATE_MSG:
            handle_private_message(client, (char *)message.payload, conn);
            break;
        case MSG_FRIEND_REQUEST:
            handle_friend_request(client, (char *)message.payload);
            break;
        case MSG_FRIEND_REQUEST_ACCEPTED:
            handle_accept_friend_request(client->socket, (char *)message.payload);
            break;
        case MSG_FRIEND_REQUEST_DECLINED:
            handle_decline_friend_request(client->socket, (char *)message.payload);
            break;
        case MSG_FRIEND_REMOVED:
            handle_remove_friend(client->socket, (char *)message.payload);
            break;
        case MSG_FRIENDS_LIST:
            handle_get_friends_list(client->socket);
            break;
        case MSG_FRIEND_REQUEST_LIST:
            handle_see_friend_request(client->socket);
            break;
        case MSG_LOGOUT:
            printf("User logging out\n");
            handle_logout(client->socket, conn); // Update last_offline_at
            break;
        case MSG_DISCONNECT:
            printf("User disconnecting\n");
            handle_disconnect(client->socket, conn); // Handle client disconnection
            goto cleanup;
        case MSG_CREATE_GROUP:
            handle_create_group(client, (char *)message.payload);
            break;
        case MSG_JOIN_GROUP:
            handle_join_group(client, (char *)message.payload);
            break;
        case MSG_GROUP_MSG:
        {
            char *group_name = strtok((char *)message.payload, ":");
            char *msg = strtok(NULL, "");
            handle_group_message(client, group_name, msg);
            break;
        }
        case MSG_LIST_GROUPS:
            handle_list_groups(client);
            break;
        case MSG_GROUP_MSG_HISTORY:
            handle_see_group_messages(client, (char *)message.payload);
            break;
        case MSG_LEAVE_GROUP:
            handle_leave_group(client, (char *)message.payload);
            break;
        case MSG_REMOVE_GROUP_MEMBER:
        {
            char *group_name = strtok((char *)message.payload, ":");
            char *member = strtok(NULL, "");
            handle_remove_group_member(client, group_name, member);
            break;
        }
        case MSG_PRIVATE_MSG_HISTORY:
            handle_see_private_messages(client, (char *)message.payload, conn);
            break;
        }
    }

cleanup:
    // Remove the client from the online list if logged in
    if (client->is_logged_in)
    {
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

    PQfinish(conn);
    close(client->socket);
    remove_client(client->socket);
    free(client);
    pthread_exit(NULL);
}

// Khởi tạo server và chấp nhận kết nối
int main()
{
    load_env_file(".env");
    // Access the required environment variables
    const char *db_url = getenv("DB_URL");

    fprintf(stdout, "DB_URL: %s\n", db_url);
    if (!db_url)
    {
        fprintf(stderr, "Missing required environment variable DB_URL\n");
        return 1;
    }

    // Connect to the PostgreSQL database
    PGconn *conn = PQconnectdb(db_url);
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }

    printf("Connected to the PostgreSQL database.\n");
    printf("Welcome to the login/register system.\n");

    init_db(conn);

    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len)) >= 0)
    {
        printf("New connection: %d\n", new_socket);
        Client *client = (Client *)malloc(sizeof(Client));
        client->socket = new_socket;
        client->address = address;
        client->conn = conn;
        add_client(client);

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void *)client) != 0)
        {
            perror("Thread creation failed");
            free(client);
        }
    }

    close(server_fd);
    return 0;
}
