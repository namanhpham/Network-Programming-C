#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol.c"
#include "protocol.h"
#include "client_gui.h"
#include "client_gui.c"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

extern GtkWidget *status_label; // Add this line

int is_logged_in = 0;
pthread_mutex_t login_mutex = PTHREAD_MUTEX_INITIALIZER; // Initialize mutex

void update_status_label(const char *message)
{
    gtk_label_set_text(GTK_LABEL(status_label), message);
}

// Function to continuously receive messages from the server
void *receive_messages(void *arg)
{
    int sockfd = *(int *)arg;
    Message msg;

    while (1)
    {
        if (receive_message(sockfd, &msg) <= 0)
        {
            printf("Server disconnected\n");
            update_status_label("Server disconnected");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Process received message
        pthread_mutex_lock(&login_mutex); // Lock mutex before updating is_logged_in
        switch (msg.type)
        {
        case RESP_SUCCESS:
            printf("Login successful!\n");
            is_logged_in = 1;
            update_status_label("Login successful!");
            show_home_interface();
            break;
        case RESP_FAILURE:
            printf("Login or registration failed.\n");
            is_logged_in = 0;
            update_status_label("Login or registration failed.");
            break;
        case MSG_ONLINE_USERS:
            printf("Online users: %s\n", (char *)msg.payload);
            update_status_label((char *)msg.payload);
            break;
        case MSG_FRIEND_REQUEST:
        {
            printf("You have friend request from: %s\n", (char *)msg.payload);
            update_status_label((char *)msg.payload);
            break;
        }
        case MSG_FRIENDS_LIST:
        {
            printf("Friend requests: %s\n", (char *)msg.payload);
            update_status_label((char *)msg.payload);
            break;
        }
        case MSG_FRIEND_REQUEST_LIST:
        {
            printf("Friend requests: %s\n", (char *)msg.payload);
            update_status_label((char *)msg.payload);
            break;
        }
        default:
            printf("Unknown message type received.\n");
            update_status_label("Unknown message type received.");
            break;
        }
        pthread_mutex_unlock(&login_mutex); // Unlock after updating
    }
    return NULL;
}

// Hàm kết nối đến server
int connect_to_server()
{
    int sockfd;
    struct sockaddr_in server_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

// Hàm đăng ký
void register_user(int sockfd)
{
    char username[128], password[128];

    // Yêu cầu người dùng nhập tên đăng nhập và mật khẩu
    printf("Enter username for registration: ");
    scanf("%127s", username);
    printf("Enter password for registration: ");
    scanf("%127s", password);

    // Tạo payload từ tên đăng nhập và mật khẩu
    char payload[256];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);

    // Tạo thông điệp đăng ký và gửi tới server
    Message msg = create_message(MSG_REGISTER, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Registration failed");
    }
}

// Hàm đăng nhập
void login_user(int sockfd)
{
    char username[128], password[128];

    // Prompt the user for username and password
    printf("Enter username for login: ");
    scanf("%127s", username);
    printf("Enter password for login: ");
    scanf("%127s", password);

    // Create the payload in the format "username:password"
    char payload[256];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);

    // Create the login message and print its contents
    Message msg = create_message(MSG_LOGIN, (uint8_t *)payload, strlen(payload));
    // printf("Message type: %d, Message payload: '%s'\n", msg.type, (char *)msg.payload);

    // Send the message to the server and check for errors
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Login failed");
        return;
    }

    // Delay to allow `receive_messages` to update `is_logged_in`
    struct timespec delay = {0, 200000000L}; // 200ms
    nanosleep(&delay, NULL);

    return;
}

// Remove redundant logout_user function
// void logout_user(int sockfd)
// {
//     Message msg = create_message(MSG_LOGOUT, (uint8_t *)"Logout", 6);
//     if (send_message(sockfd, &msg) < 0)
//     {
//         perror("Logout failed");
//     }
//     else
//     {
//         pthread_mutex_lock(&login_mutex);
//         is_logged_in = 0;
//         pthread_mutex_unlock(&login_mutex);
//         printf("Logged out successfully.\n");
//     }
// }

void create_group(int sockfd)
{
    char group_name[128];
    printf("Enter group name: ");
    scanf("%127s", group_name);

    Message msg = create_message(MSG_CREATE_GROUP, (uint8_t *)group_name, strlen(group_name));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Create group failed");
    }
    else
    {
        printf("Group created successfully.\n");
    }
}

void join_group(int sockfd)
{
    char group_name[128];
    printf("Enter group name to join: ");
    scanf("%127s", group_name);

    Message msg = create_message(MSG_JOIN_GROUP, (uint8_t *)group_name, strlen(group_name));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Join group failed");
    }
    else
    {
        printf("Joined group successfully.\n");
    }
}

void send_group_message(int sockfd)
{
    char group_name[128], message[256];
    printf("Enter group name: ");
    scanf("%127s", group_name);
    getchar(); // Clear input buffer
    printf("Enter message: ");
    fgets(message, sizeof(message), stdin);

    char payload[512];
    snprintf(payload, sizeof(payload), "%s:%s", group_name, message);
    Message msg = create_message(MSG_GROUP_MSG, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Send group message failed");
    }
}

void send_private_message(int sockfd)
{
    char receiver_username[128], message_content[512];
    printf("Enter receiver username: ");
    scanf("%127s", receiver_username);
    getchar(); // Clear input buffer
    printf("Enter message: ");
    fgets(message_content, sizeof(message_content), stdin);

    char payload[640];
    snprintf(payload, sizeof(payload), "%s:%s", receiver_username, message_content);
    Message msg = create_message(MSG_PRIVATE_MSG, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Send private message failed");
    }
}

void disconnect(int sockfd)
{
    Message msg = create_message(MSG_DISCONNECT, (uint8_t *)"Disconnecting", 12);
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Failed to send disconnect message");
    }
}

void send_friend_request(int sockfd)
{
    char friend_username[128];
    printf("Enter friend username: ");
    scanf("%127s", friend_username);

    char payload[256];
    snprintf(payload, sizeof(payload), "%s", friend_username);

    Message msg = create_message(MSG_FRIEND_REQUEST, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Failed to send friend request");
    }
}

void see_friend_requests(int sockfd)
{
    Message msg = create_message(MSG_FRIEND_REQUEST_LIST, (uint8_t *)"Friend requests", 15);
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Failed to see friend requests");
    }
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    int sockfd = connect_to_server();
    pthread_t recv_thread;

    if (pthread_create(&recv_thread, NULL, receive_messages, &sockfd) != 0)
    {
        perror("Failed to create receive thread");
        close(sockfd);
        return EXIT_FAILURE;
    }

    create_gui(&sockfd);

    gtk_main();

    close(sockfd);
    return 0;
}