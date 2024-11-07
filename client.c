#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol.c"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int is_logged_in = 0;

// Function to continuously receive messages from the server
void *receive_messages(void *arg) {
    int sockfd = *(int *)arg;
    Message msg;

    while (1) {
        if (receive_message(sockfd, &msg) <= 0) {
            printf("Server disconnected\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Process received message
        switch (msg.type) {
            case RESP_SUCCESS:
                printf("Login successful!\n");
                is_logged_in = 1;
                break;
            case RESP_FAILURE:
                printf("Login or registration failed.\n");
                break;
            case MSG_ONLINE_USERS:
                printf("Online users: %s\n", (char *)msg.payload);
                break;
            default:
                printf("Unknown message type received.\n");
                break;
        }
    }
    return NULL;
}

// Hàm kết nối đến server
int connect_to_server() {
    int sockfd;
    struct sockaddr_in server_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

// Hàm đăng ký
// Hàm đăng ký
void register_user(int sockfd) {
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
    if (send_message(sockfd, &msg) < 0) {
        perror("Registration failed");
    }
}

// Hàm đăng nhập
void login_user(int sockfd) {
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
    if (send_message(sockfd, &msg) < 0) {
        perror("Login failed");
        return;
    }
    is_logged_in = 1;
    return;
}

int main() {
    int sockfd = connect_to_server();
    pthread_t recv_thread;

    // Create a thread to receive messages from the server
    if (pthread_create(&recv_thread, NULL, receive_messages, &sockfd) != 0) {
        perror("Failed to create receive thread");
        close(sockfd);
        return EXIT_FAILURE;
    }

    int command;
    while (1) {
        if(is_logged_in) {
            printf("Enter command (3: send message, 4: exit): ");
        } else {
            printf("Enter command (1: register, 2: login, 3: send message, 4: exit): ");
        }
        scanf("%d", &command);
        // Xóa bộ đệm nhập liệu
        while (getchar() != '\n'); // Xóa tất cả ký tự dư trong buffer cho đến khi gặp '\n'

        switch (command) {
            case 1:
                if(is_logged_in) {
                    printf("You must logout first\n");
                    break;
                }
                register_user(sockfd);
                break;
            case 2:
                if(is_logged_in) {
                    printf("You are already logged in\n");
                    break;
                }
                login_user(sockfd);
                break;
            case 3:
                if(!is_logged_in) {
                    printf("You must login first\n");
                    break;
                }
                send_message_example(sockfd);
                break;
            case 4:
                close(sockfd);
                return 0;
            default:
                printf("Unknown command\n");
        }
    }

    close(sockfd);
    return 0;
}
