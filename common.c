#include "common.h" // Include nếu kiểu Client được định nghĩa ở đây, hoặc định nghĩa Client nếu chưa có.
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

int username_exists(const char *username)
{
    FILE *file = fopen(ACCOUNTS_FILE, "r");
    if (!file)
    {
        perror("Failed to open accounts file");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        char *stored_username = strtok(line, ":");
        if (stored_username && strcmp(stored_username, username) == 0)
        {
            fclose(file);
            return 1; // Username đã tồn tại
        }
    }

    fclose(file);
    return 0; // Username chưa tồn tại
}

int save_account(const char *username, const char *password)
{
    FILE *file = fopen(ACCOUNTS_FILE, "a");
    if (!file)
    {
        perror("Failed to open accounts file");
        return 0;
    }

    fprintf(file, "%s:%s\n", username, password);
    fclose(file);
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

int validate_credentials(const char *username, const char *password)
{
    FILE *file = fopen(ACCOUNTS_FILE, "r");
    if (!file)
    {
        perror("Failed to open accounts file");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        // Split username and password
        char *stored_username = strtok(line, ":");
        char *stored_password = strtok(NULL, "\n");

        // Trim any newline or whitespace characters
        if (stored_username)
            trim_newline(stored_username);
        if (stored_password)
            trim_newline(stored_password);

        // Check if credentials match
        if (stored_username && stored_password &&
            strcmp(stored_username, username) == 0 &&
            strcmp(stored_password, password) == 0)
        {
            fclose(file);
            return 1; // Login successful
        }
    }

    fclose(file);
    return 0; // Login failed
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