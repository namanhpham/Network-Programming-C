#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol.c"

#define PORT 8080
#define MAX_CLIENTS 10
#define ACCOUNTS_FILE "accounts.txt"

// Struct lưu thông tin cho mỗi client
typedef struct {
    int socket;
    struct sockaddr_in address;
    int user_id; // Để theo dõi đăng nhập cho từng người dùng
} Client;

Client *clients[MAX_CLIENTS];


// Hàm để thêm client vào danh sách
void add_client(Client *client) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = client;
            break;
        }
    }
}

// Hàm xóa client khỏi danh sách
void remove_client(int socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == socket) {
            clients[i] = NULL;
            break;
        }
    }
}

// Hàm kiểm tra và tạo file accounts.txt nếu chưa tồn tại
void check_and_create_accounts_file() {
    FILE *file = fopen(ACCOUNTS_FILE, "a"); // Mở ở chế độ append, tạo file nếu chưa tồn tại
    if (!file) {
        perror("Failed to open or create accounts file");
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

// Hàm kiểm tra tên đăng nhập đã tồn tại trong file hay chưa
int username_exists(const char *username) {
    FILE *file = fopen(ACCOUNTS_FILE, "r");
    if (!file) {
        perror("Failed to open accounts file");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *stored_username = strtok(line, ":");
        if (stored_username && strcmp(stored_username, username) == 0) {
            fclose(file);
            return 1; // Username đã tồn tại
        }
    }

    fclose(file);
    return 0; // Username chưa tồn tại
}

// Hàm lưu thông tin tài khoản mới vào file
int save_account(const char *username, const char *password) {
    FILE *file = fopen(ACCOUNTS_FILE, "a");
    if (!file) {
        perror("Failed to open accounts file");
        return 0;
    }

    fprintf(file, "%s:%s\n", username, password);
    fclose(file);
    return 1;
}

// Hàm xử lý yêu cầu đăng ký từ client
void handle_register(int client_socket, const char *payload) {
    char username[128], password[128];
    sscanf(payload, "%127[^:]:%127s", username, password); // Tách username và password

    if (username_exists(username)) {
        // Gửi phản hồi nếu tên người dùng đã tồn tại
        printf("Username %s already exists\n", username);
        Message response = { RESP_FAILURE, {0} };
        send_message(client_socket, &response);
    } else {
        // Lưu tài khoản mới và gửi phản hồi thành công
        if (save_account(username, password)) {
            printf("User %s registered successfully\n", username);
            Message response = { RESP_SUCCESS, {0} };
            send_message(client_socket, &response);
        } else {
            // Gửi phản hồi thất bại nếu có lỗi khi lưu
            printf("Failed to register user %s\n", username);
            Message response = { RESP_FAILURE, {0} };
            send_message(client_socket, &response);
        }
    }
}

// Hàm xử lý yêu cầu từ client
void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    Message message;
    ssize_t received;

    while ((received = receive_message(client->socket, &message)) > 0) {
        switch (message.type) {
            case MSG_REGISTER:
                handle_register(client->socket, (char *)message.payload);
                break;
            case MSG_LOGIN:
                printf("User login\n");
                client->user_id = 1; // Giả sử đăng nhập thành công
                send_message(client->socket, &(Message){RESP_SUCCESS, {0}});
                break;
            case MSG_PRIVATE_MSG:
                printf("Private message received\n");
                break;
            case MSG_DISCONNECT:
                printf("User disconnecting\n");
                goto cleanup;
        }
    }

cleanup:
    close(client->socket);
    remove_client(client->socket);
    free(client);
    pthread_exit(NULL);
}

// Khởi tạo server và chấp nhận kết nối
int main() {

    check_and_create_accounts_file();

    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len)) >= 0) {
        printf("New connection: %d\n", new_socket);
        Client *client = (Client *)malloc(sizeof(Client));
        client->socket = new_socket;
        client->address = address;
        add_client(client);

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void *)client) != 0) {
            perror("Thread creation failed");
            free(client);
        }
    }

    close(server_fd);
    return 0;
}
