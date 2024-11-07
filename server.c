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

// Function to validate credentials from the file
// Helper function to trim newline and other whitespace characters
void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
    }
}

// Function to print all accounts from the accounts file for debugging
void print_accounts() {
    FILE *file = fopen(ACCOUNTS_FILE, "r");
    if (!file) {
        perror("Failed to open accounts file");
        return;
    }

    printf("Accounts in %s:\n", ACCOUNTS_FILE);
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Print each raw line and then the processed username/password for visibility
        printf("Raw line: %s", line);

        // Split the line and trim newline for clarity
        char *username = strtok(line, ":");
        char *password = strtok(NULL, "\n");
        if (username) trim_newline(username);
        if (password) trim_newline(password);

        printf("Parsed username: '%s', password: '%s'\n", username ? username : "NULL", password ? password : "NULL");
    }

    fclose(file);
}

// Function to validate credentials from the file
int validate_credentials(const char *username, const char *password) {
    print_accounts();  // Debug: print accounts for visibility
    FILE *file = fopen(ACCOUNTS_FILE, "r");
    if (!file) {
        perror("Failed to open accounts file");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Split username and password
        char *stored_username = strtok(line, ":");
        char *stored_password = strtok(NULL, "\n");

        // Trim any newline or whitespace characters
        if (stored_username) trim_newline(stored_username);
        if (stored_password) trim_newline(stored_password);

        // Check if credentials match
        if (stored_username && stored_password &&
            strcmp(stored_username, username) == 0 &&
            strcmp(stored_password, password) == 0) {
            fclose(file);
            return 1; // Login successful
        }
    }

    fclose(file);
    return 0; // Login failed
}


// Modify handle_login function to validate credentials
void handle_login(int client_socket, const char *payload) {
    char username[128], password[128];
    sscanf(payload, "%127[^:]:%127s", username, password); // Split username and password

    if (validate_credentials(username, password)) {
        printf("User %s logged in successfully\n", username);
        Message response = { RESP_SUCCESS, {0} };
        send_message(client_socket, &response);
    } else {
        printf("Invalid credentials for user %s\n", username);
        Message response = { RESP_FAILURE, {0} };
        send_message(client_socket, &response);
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
                handle_login(client->socket, (char *)message.payload);
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
