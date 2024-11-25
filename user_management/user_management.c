#include <string.h>
#include <stdio.h>
#include "user_management.h"
#include "../protocol.h" // Assuming protocol handling is also separated
#include "../common.h"

// Hàm xử lý yêu cầu đăng ký từ client
void handle_register(int client_socket, const char *payload)
{
    char username[128], password[128];
    sscanf(payload, "%127[^:]:%127s", username, password); // Tách username và password

    if (username_exists(username))
    {
        // Gửi phản hồi nếu tên người dùng đã tồn tại
        printf("Username %s already exists\n", username);
        Message response = {RESP_FAILURE, {0}};
        send_message(client_socket, &response);
    }
    else
    {
        // Lưu tài khoản mới và gửi phản hồi thành công
        if (save_account(username, password))
        {
            printf("User %s registered successfully\n", username);
            Message response = {RESP_SUCCESS, {0}};
            send_message(client_socket, &response);
        }
        else
        {
            // Gửi phản hồi thất bại nếu có lỗi khi lưu
            printf("Failed to register user %s\n", username);
            Message response = {RESP_FAILURE, {0}};
            send_message(client_socket, &response);
        }
    }
}

void handle_login(int client_socket, const char *payload)
{
    char username[128], password[128];
    sscanf(payload, "%127[^:]:%127s", username, password); // Split username and password

    // Check if the user is already logged in
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (online_clients[i] && strcmp(online_clients[i]->username, username) == 0)
        {
            printf("User %s is already logged in\n", username);
            Message response = {RESP_FAILURE, {0}};
            send_message(client_socket, &response);
            return;
        }
    }

    // Validate credentials
    if (validate_credentials(username, password))
    {
        printf("User %s logged in successfully\n", username);

        // Update client's status and add to online list
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] && clients[i]->socket == client_socket)
            {
                strcpy(clients[i]->username, username);
                clients[i]->is_logged_in = 1;
                add_online_client(clients[i]);
                break;
            }
        }

        Message response = {RESP_SUCCESS, {0}};
        send_message(client_socket, &response);

        // Send the updated online users list to all clients
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (online_clients[i] && online_clients[i]->is_logged_in)
            {
                send_online_users_list(online_clients[i]->socket);
            }
        }
    }
    else
    {
        printf("Invalid credentials for user %s\n", username);
        Message response = {RESP_FAILURE, {0}};
        send_message(client_socket, &response);
    }
}

void handle_logout(int client_socket)
{
    // Logout logic
}
