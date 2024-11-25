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
#include "protocol.h"
#include "common.h"
#include <libpq-fe.h>

#define DB_CONN_INFO "host=localhost dbname=ltm user=postgres password=postgres"
#define PORT 8080
#define MAX_FRIENDS 100 // Định nghĩa số lượng bạn bè tối đa (có thể điều chỉnh)
#define ACCOUNTS_FILE "accounts.txt"
#define FRIEND_REQUEST_FILE "friend_requests.txt"

typedef struct {
    char name[128];
    Client *members[MAX_CLIENTS];
} Group;
Group *groups[MAX_CLIENTS] = {0};

// Remove client from online list
void remove_online_client(Client *client)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (online_clients[i] == client)
        {
            online_clients[i] = NULL;
            break;
        }
    }
}

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

// Hàm kiểm tra và tạo file accounts.txt nếu chưa tồn tại
void check_and_create_accounts_file()
{
    FILE *file = fopen(ACCOUNTS_FILE, "a"); // Mở ở chế độ append, tạo file nếu chưa tồn tại
    if (!file)
    {
        perror("Failed to open or create accounts file");
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

void save_friend_request(const char *from_username, const char *to_username)
{
    FILE *file = fopen(FRIEND_REQUEST_FILE, "a");
    if (!file)
    {
        perror("Failed to open friend requests file");
        return;
    }

    fprintf(file, "%s:%s\n", from_username, to_username);
    fclose(file);
}


void handle_friend_request(int client_socket, const char *payload)
{
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket);
    if (client)
    {
        // Check if the friend exists and is online
        if (is_online(friend_username))
        {
            // Send the friend request to the friend
            Client *friend_client = get_online_client_by_username(friend_username);
            if (friend_client)
            {
                Message friend_request_msg = create_message(MSG_FRIEND_REQUEST, (uint8_t *)client->username, strlen(client->username));
                send_message(friend_client->socket, &friend_request_msg);
            }
            save_friend_request(client->username, friend_username);
        }
    }
}

void handle_see_friend_request(int client_socket)
{
    Client *client = get_client_by_socket(client_socket);
    if (client)
    {
        FILE *file = fopen(FRIEND_REQUEST_FILE, "r");
        if (!file)
        {
            perror("Failed to open friend requests file");
            return;
        }
        printf("read file\n");

        char line[256];
        while (fgets(line, sizeof(line), file))
        {
            char *from_username = strtok(line, ":");
            char *to_username = strtok(NULL, "\n");
            if (from_username && strcmp(to_username, client->username) == 0)
            {
                Message friend_request_msg = create_message(MSG_FRIEND_REQUEST_LIST, (uint8_t *)from_username, strlen(from_username));
                send_message(client_socket, &friend_request_msg);
            }
        }

        fclose(file);
    }
}
void handle_group_message(Client *client, const char *group_name, const char *message) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (groups[i] && strcmp(groups[i]->name, group_name) == 0) {
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (groups[i]->members[j] && groups[i]->members[j] != client) {
                    Message msg = create_message(MSG_GROUP_MSG, (uint8_t *)message, strlen(message));
                    send_message(groups[i]->members[j]->socket, &msg);
                }
            }
            return;
        }
    }
}
void handle_create_group(Client *client, const char *group_name) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (groups[i] == NULL) {
            groups[i] = malloc(sizeof(Group));
            strncpy(groups[i]->name, group_name, sizeof(groups[i]->name));
            groups[i]->members[0] = client;
            printf("Group '%s' created by %s\n", group_name, client->username);
            return;
        }
    }
}
void handle_join_group(Client *client, const char *group_name) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (groups[i] && strcmp(groups[i]->name, group_name) == 0) {
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (groups[i]->members[j] == NULL) {
                    groups[i]->members[j] = client;
                    printf("%s joined group '%s'\n", client->username, group_name);
                    return;
                }
            }
        }
    }
    printf("Group '%s' not found.\n", group_name);
}

void *handle_client(void *arg)
{
    Client *client = (Client *)arg;
    Message message;
    ssize_t received;

    while ((received = receive_message(client->socket, &message)) > 0)
    {
        switch (message.type)
        {
        case MSG_REGISTER:
            handle_register(client->socket, (char *)message.payload);
            break;
        case MSG_LOGIN:
            handle_login(client->socket, (char *)message.payload);
            break;
        case MSG_PRIVATE_MSG:
        {
            // payload định dạng: <username_nhan>:<noi_dung>
            char receiver_username[128], message_content[512];
            sscanf((char *)message.payload, "%127[^:]:%511s", receiver_username, message_content);

            // Tìm người nhận trong danh sách online_clients
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (online_clients[i] && strcmp(online_clients[i]->username, receiver_username) == 0)
                {
                    // Gửi tin nhắn cho người nhận
                    Message msg = create_message(MSG_PRIVATE_MSG, (uint8_t *)message_content, strlen(message_content));
                    send_message(online_clients[i]->socket, &msg);
                    break;
                }
            }
            break;
        }
        case MSG_FRIEND_REQUEST:
        {
            // payload định dạng: <username_nhan>
            char friend_username[128];
            sscanf((char *)message.payload, "%127s", friend_username);

            // Tìm người nhận trong danh sách online_clients
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (online_clients[i] && strcmp(online_clients[i]->username, friend_username) == 0)
                {
                    Message friend_request_msg = create_message(MSG_FRIEND_REQUEST, (uint8_t *)client->username, strlen(client->username));
                    send_message(online_clients[i]->socket, &friend_request_msg);
                    // Save to friend request file
                    save_friend_request(client->username, friend_username);
                    break;
                }
            }
            break;
        }
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
        case MSG_DISCONNECT:
            printf("User disconnecting\n");
            goto cleanup;
        case MSG_CREATE_GROUP:
                handle_create_group(client, (char *)message.payload);
                break;
        case MSG_JOIN_GROUP:
            handle_join_group(client, (char *)message.payload);
            break;
        case MSG_GROUP_MSG: {
            char *group_name = strtok((char *)message.payload, ":");
            char *msg = strtok(NULL, "");
            handle_group_message(client, group_name, msg);
            break;
        }
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

    close(client->socket);
    remove_client(client->socket);
    free(client);
    pthread_exit(NULL);
}

// Khởi tạo server và chấp nhận kết nối
int main()
{
    PGconn *conn;

    // Connect to the database
    conn = PQconnectdb(DB_CONN_INFO);
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }

    printf("Connected to the PostgreSQL database.\n");
    printf("Welcome to the login/register system.\n");
    // TEST DB
// // Create a table
//     PGresult *res = PQexec(conn, "CREATE TABLE IF NOT EXISTS users (id SERIAL PRIMARY KEY, name VARCHAR(255), age INT)");
//     if (PQresultStatus(res) != PGRES_COMMAND_OK) {
//         fprintf(stderr, "CREATE TABLE failed: %s", PQerrorMessage(conn));
//         PQclear(res);
//         PQfinish(conn);
//         exit(EXIT_FAILURE);
//     }
//     PQclear(res);

//     // Insert data
//     res = PQexec(conn, "INSERT INTO users (name, age) VALUES ('Alice', 30), ('Bob', 25)");
//     if (PQresultStatus(res) != PGRES_COMMAND_OK) {
//         fprintf(stderr, "INSERT failed: %s", PQerrorMessage(conn));
//         PQclear(res);
//         PQfinish(conn);
//         exit(EXIT_FAILURE);
//     }
//     PQclear(res);

//     // Fetch data
//     res = PQexec(conn, "SELECT id, name, age FROM users");
//     if (PQresultStatus(res) != PGRES_TUPLES_OK) {
//         fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
//         PQclear(res);
//         PQfinish(conn);
//         exit(EXIT_FAILURE);
//     }

//     // Print fetched data
//     int rows = PQntuples(res);
//     for (int i = 0; i < rows; i++) {
//         printf("ID: %s, Name: %s, Age: %s\n", PQgetvalue(res, i, 0), PQgetvalue(res, i, 1), PQgetvalue(res, i, 2));
//     }

//     PQclear(res);
//     PQfinish(conn);

//     printf("Done!\n");
//     return EXIT_SUCCESS;
    check_and_create_accounts_file();

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
