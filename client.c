#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol.h" // Change from "protocol.c" to "protocol.h"

// Function prototype for send_message_example
void send_message_example(int sockfd);

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int is_logged_in = 0;
pthread_mutex_t login_mutex = PTHREAD_MUTEX_INITIALIZER; // Initialize mutex

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
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Process received message
        pthread_mutex_lock(&login_mutex); // Lock mutex before updating is_logged_in
        switch (msg.type) {
            case RESP_REGISTER_SUCCESS:
                printf("Registration successful!\n");
                break;
            case RESP_LOGIN_SUCCESS:
                is_logged_in = 1;
                break;
            case RESP_LOGIN_FAILURE:
                printf("Login or registration failed.\n");
                is_logged_in = 0;
                break;
            case RESP_SUCCESS:
                printf("Success: %s\n", (char *)msg.payload);
                break;
            case RESP_FAILURE:
                printf("Failure: %s\n", (char *)msg.payload);
                break;
            case MSG_ONLINE_USERS:
                printf("Online users: %s\n", (char *)msg.payload);
                break;
            case MSG_FRIEND_REQUEST: {
                printf("You have friend request from: %s\n", (char *)msg.payload);
                break;
            }
            case MSG_FRIENDS_LIST: {
                printf("Friend requests: %s\n", (char *)msg.payload);
                break;
            }
            case MSG_FRIEND_REQUEST_LIST: {
                printf("Friend requests: %s\n", (char *)msg.payload);
                break;
            }
            case MSG_GROUP_MSG_HISTORY:
            {
                printf("%s", (char *)msg.payload);
                break;
            }
            case MSG_GROUP_MSG:
            {
                printf("%s\n", (char *)msg.payload);
                break;
            }
            case RESP_LEAVE_GROUP:
            {
                printf("%s\n", (char *)msg.payload);
                break;
            }
            case RESP_REMOVE_GROUP_MEMBER:
            {
                printf("%s\n", (char *)msg.payload);
                break;
            }
                default:
                    printf("Unknown message type received.\n");
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
    struct timespec delay = {0, 500000000L}; // 500ms
    nanosleep(&delay, NULL);

    return;
}

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

void logout_user(int sockfd)
{
    Message msg = create_message(MSG_LOGOUT, (uint8_t *)"Logout", 6);
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Logout failed");
    }
    else
    {
        pthread_mutex_lock(&login_mutex);
        is_logged_in = 0;
        pthread_mutex_unlock(&login_mutex);
        printf("Logged out successfully.\n");
    }
}

void see_group_messages(int sockfd)
{
    char group_name[128];
    printf("Enter group name to see messages: ");
    scanf("%127s", group_name);

    Message msg = create_message(MSG_GROUP_MSG_HISTORY, (uint8_t *)group_name, strlen(group_name));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Failed to see group messages");
    }
}

void list_groups(int sockfd)
{
    Message msg = create_message(MSG_LIST_GROUPS, (uint8_t *)"List groups", 11);
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Failed to list groups");
    }
}

void leave_group(int sockfd)
{
    char group_name[128];
    printf("Enter group name to leave: ");
    scanf("%127s", group_name);

    Message msg = create_message(MSG_LEAVE_GROUP, (uint8_t *)group_name, strlen(group_name));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Failed to leave group");
    }
}

void remove_group_members(int sockfd)
{
    char group_name[128], member_username[128];
    printf("Enter group name: ");
    scanf("%127s", group_name);
    printf("Enter member username: ");
    scanf("%127s", member_username);

    char payload[256];
    snprintf(payload, sizeof(payload), "%s:%s", group_name, member_username);

    Message msg = create_message(MSG_REMOVE_GROUP_MEMBER, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        perror("Failed to remove group members");
    }
}
int main()
{
    int sockfd = connect_to_server();
    pthread_t recv_thread;

    if (pthread_create(&recv_thread, NULL, receive_messages, &sockfd) != 0)
    {
        perror("Failed to create receive thread");
        close(sockfd);
        return EXIT_FAILURE;
    }

    int command;
    while (1)
    {
        pthread_mutex_lock(&login_mutex);
        int logged_in = is_logged_in;
        pthread_mutex_unlock(&login_mutex);

        if (logged_in)
        {
            printf("Enter command (3: send message, 4: exit, 5: add friend, 6: see friend requests, 7: create group chat, 8: join group chat, 9: send group message, 10: logout, 11: list groups, 12: see group messages, 13: leave group, 14: remove group members): ");
        }
        else
        {
            printf("Enter command (1: register, 2: login, 3: send message, 4: exit): ");
        }
        printf("main: is_logged_in = %d\n", logged_in);

        scanf("%d", &command);
        while (getchar() != '\n')
            ; // Clear input buffer

        switch (command)
        {
        case 1:
            pthread_mutex_lock(&login_mutex);
            if (is_logged_in)
            {
                printf("You must logout first\n");
                pthread_mutex_unlock(&login_mutex);
                break;
            }
            pthread_mutex_unlock(&login_mutex);
            register_user(sockfd);
            break;
        case 2:
            pthread_mutex_lock(&login_mutex);
            if (is_logged_in)
            {
                printf("You are already logged in\n");
                pthread_mutex_unlock(&login_mutex);
                break;
            }
            pthread_mutex_unlock(&login_mutex);
            login_user(sockfd);
            break;
        case 3:
            pthread_mutex_lock(&login_mutex);
            if (!is_logged_in)
            {
                printf("You must login first\n");
                pthread_mutex_unlock(&login_mutex);
                break;
            }
            pthread_mutex_unlock(&login_mutex);
            send_message_example(sockfd);
            break;
        case 4:
            disconnect(sockfd);
            return 0;
        case 5:
            send_friend_request(sockfd);
            break;
        case 6:
            see_friend_requests(sockfd);
            break;

        case 7:
            create_group(sockfd);
            break;
        case 8:
            join_group(sockfd);
            break;
        case 9:
            send_group_message(sockfd);
            break;
        case 10:
            pthread_mutex_lock(&login_mutex);
            if (!is_logged_in)
            {
                printf("You are not logged in\n");
                pthread_mutex_unlock(&login_mutex);
                break;
            }
            pthread_mutex_unlock(&login_mutex);
            logout_user(sockfd);
            break;
        case 11:
            list_groups(sockfd);
            break;
        case 12:
            see_group_messages(sockfd);
            break;
        case 13:
            leave_group(sockfd);
            break;
        case 14:
            remove_group_members(sockfd);
            break;

        default:
            printf("Unknown command\n");
        }
    }

    close(sockfd);
    return 0;
}