#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.c"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

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
    
    // Yêu cầu người dùng nhập tên đăng nhập và mật khẩu
    printf("Enter username for login: ");
    scanf("%127s", username);
    printf("Enter password for login: ");
    scanf("%127s", password);

    // Tạo payload từ tên đăng nhập và mật khẩu
    char payload[256];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);

    // Tạo thông điệp đăng nhập và gửi tới server
    Message msg = create_message(MSG_LOGIN, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0) {
        perror("Login failed");
    }
}


#include <stdio.h>
#include <string.h>
#include <unistd.h>

int connect_to_server();
void register_user(int sockfd);
void login_user(int sockfd);
void send_message_example(int sockfd);

int main() {
    int sockfd = connect_to_server();
    int command;

    while (1) {
        printf("Enter command (1: register, 2: login, 3: send message, 4: exit): ");
        scanf("%d", &command);
        // Xóa bộ đệm nhập liệu
        while (getchar() != '\n'); // Xóa tất cả ký tự dư trong buffer cho đến khi gặp '\n'

        switch (command) {
            case 1:
                register_user(sockfd);
                break;
            case 2:
                login_user(sockfd);
                break;
            case 3:
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
