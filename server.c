#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol.c"
#include <libpq-fe.h>

#define DB_CONN_INFO "host=localhost dbname=ltm user=postgres password=postgres"
#define PORT 8080
#define MAX_CLIENTS 10
#define MAX_FRIENDS 100  // Định nghĩa số lượng bạn bè tối đa (có thể điều chỉnh)
#define ACCOUNTS_FILE "accounts.txt"
#define FRIEND_REQUEST_FILE "friend_requests.txt"

typedef struct {
    char name[128];
    Client *members[MAX_CLIENTS];
} Group;

Group *groups[MAX_CLIENTS] = {0};


Client *clients[MAX_CLIENTS];

Client *online_clients[MAX_CLIENTS] = {0};

FriendPair friends[MAX_FRIENDS] = {0};

int is_online(const char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Kiểm tra nếu client ở vị trí i có username khớp và đang kết nối
        if (clients[i]->socket > 0 && strcmp(clients[i]->username, username) == 0) {
            return 1;  // Người dùng đang trực tuyến
        }
    }
    return 0;  // Người dùng không trực tuyến
}

Client* get_client_by_socket(int client_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == client_socket) {
            return clients[i];  // Return the Client pointer directly
        }
    }
    return NULL;
}

Client* get_online_client_by_username(const char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (online_clients[i] && strcmp(online_clients[i]->username, username) == 0) {
            return online_clients[i];
        }
    }
    return NULL;
}

// Thêm bạn vào danh sách bạn bè
void add_friend(const char *username1, const char *username2) {
    for (int i = 0; i < MAX_FRIENDS; i++) {
        if (strlen(friends[i].username1) == 0) {
            strcpy(friends[i].username1, username1);
            strcpy(friends[i].username2, username2);
            break;
        }
    }
}

// Kiểm tra xem hai người dùng có phải bạn bè không
int is_friend(const char *username1, const char *username2) {
    for (int i = 0; i < MAX_FRIENDS; i++) {
        if ((strcmp(friends[i].username1, username1) == 0 && strcmp(friends[i].username2, username2) == 0) ||
            (strcmp(friends[i].username1, username2) == 0 && strcmp(friends[i].username2, username1) == 0)) {
            return 1;
        }
    }
    return 0;
}

// Xóa bạn khỏi danh sách bạn bè
void remove_friend(const char *username1, const char *username2) {
    for (int i = 0; i < MAX_FRIENDS; i++) {
        if ((strcmp(friends[i].username1, username1) == 0 && strcmp(friends[i].username2, username2) == 0) ||
            (strcmp(friends[i].username1, username2) == 0 && strcmp(friends[i].username2, username1) == 0)) {
            memset(&friends[i], 0, sizeof(FriendPair)); // Xóa bạn khỏi danh sách
            break;
        }
    }
}

// Xử lý yêu cầu chấp nhận kết bạn
void handle_accept_friend_request(int client_socket, const char *payload) {
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket); // Hàm giả định trả về client từ socket

    if (client && !is_friend(client->username, friend_username)) {
        add_friend(client->username, friend_username);

        // Gửi thông báo cho người gửi lời mời
        Client *friend_client = get_online_client_by_username(friend_username); // Hàm trả về client theo username
        if (friend_client) {
            Message response = create_message(MSG_FRIEND_REQUEST_ACCEPTED, (uint8_t *)client->username, strlen(client->username));
            send_message(friend_client->socket, &response);
        }
    }
}

// Xử lý yêu cầu từ chối kết bạn
void handle_decline_friend_request(int client_socket, const char *payload) {
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket);

    if (client) {
        Client *friend_client = get_online_client_by_username(friend_username);
        if (friend_client) {
            Message response = create_message(MSG_FRIEND_REQUEST_DECLINED, (uint8_t *)client->username, strlen(client->username));
            send_message(friend_client->socket, &response);
        }
    }
}

// Xử lý yêu cầu hủy kết bạn
void handle_remove_friend(int client_socket, const char *payload) {
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket);
    if (client && is_friend(client->username, friend_username)) {
        remove_friend(client->username, friend_username);

        // Thông báo cho cả hai bên về việc hủy kết bạn
        Client *friend_client = get_online_client_by_username(friend_username);
        if (friend_client) {
            Message response = create_message(MSG_FRIEND_REMOVED, (uint8_t *)client->username, strlen(client->username));
            send_message(friend_client->socket, &response);
        }
    }
}

// Xử lý yêu cầu lấy danh sách bạn bè
void handle_get_friends_list(int client_socket) {
    Client *client = get_client_by_socket(client_socket);
    if (client) {
        char friends_list[1024] = {0};
        
        Message response = create_message(MSG_FRIENDS_LIST, (uint8_t *)friends_list, strlen(friends_list));
        send_message(client_socket, &response);
    }
}

// Add client to online list
void add_online_client(Client *client) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (online_clients[i] == NULL) {
            online_clients[i] = client;
            break;
        }
    }
}

// Remove client from online list
void remove_online_client(Client *client) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (online_clients[i] == client) {
            online_clients[i] = NULL;
            break;
        }
    }
}

void send_online_users_list(int client_socket) {
    char online_users[1024] = {0};

    // Create a comma-separated list of online usernames
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (online_clients[i] && online_clients[i]->is_logged_in) {
            strcat(online_users, online_clients[i]->username);
            strcat(online_users, ",");
        }
    }
    
    // Remove the trailing comma, if any
    size_t len = strlen(online_users);
    if (len > 0) {
        online_users[len - 1] = '\0';
    }

    // Send the list to the specified client
    Message msg = create_message(MSG_ONLINE_USERS, (uint8_t *)online_users, strlen(online_users));
    send_message(client_socket, &msg);
}


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

void save_friend_request(const char *from_username, const char *to_username) {
    FILE *file = fopen(FRIEND_REQUEST_FILE, "a");
    if (!file) {
        perror("Failed to open friend requests file");
        return;
    }

    fprintf(file, "%s:%s\n", from_username, to_username);
    fclose(file);
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

// Function to validate credentials from the file
int validate_credentials(const char *username, const char *password) {
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


void handle_login(int client_socket, const char *payload) {
    char username[128], password[128];
    sscanf(payload, "%127[^:]:%127s", username, password); // Split username and password

    // Check if the user is already logged in
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (online_clients[i] && strcmp(online_clients[i]->username, username) == 0) {
            printf("User %s is already logged in\n", username);
            Message response = { RESP_FAILURE, {0} };
            send_message(client_socket, &response);
            return;
        }
    }

    // Validate credentials
    if (validate_credentials(username, password)) {
        printf("User %s logged in successfully\n", username);

        // Update client's status and add to online list
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] && clients[i]->socket == client_socket) {
                strcpy(clients[i]->username, username);
                clients[i]->is_logged_in = 1;
                add_online_client(clients[i]);
                break;
            }
        }

        Message response = { RESP_SUCCESS, {0} };
        send_message(client_socket, &response);

        // Send the updated online users list to all clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (online_clients[i] && online_clients[i]->is_logged_in) {
                send_online_users_list(online_clients[i]->socket);
            }
        }
    } else {
        printf("Invalid credentials for user %s\n", username);
        Message response = { RESP_FAILURE, {0} };
        send_message(client_socket, &response);
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


void handle_friend_request(int client_socket, const char *payload) {
    char friend_username[128];
    sscanf(payload, "%127s", friend_username);

    Client *client = get_client_by_socket(client_socket);
    if (client) {
        // Check if the friend exists and is online
        if (is_online(friend_username)) {
            // Send the friend request to the friend
            Client *friend_client = get_online_client_by_username(friend_username);
            if (friend_client) {
                Message friend_request_msg = create_message(MSG_FRIEND_REQUEST, (uint8_t *)client->username, strlen(client->username));
                send_message(friend_client->socket, &friend_request_msg);
            }
            save_friend_request(client->username, friend_username);
        }
    }
}

void handle_see_friend_request(int client_socket) {
    Client *client = get_client_by_socket(client_socket);
    if (client) {
        FILE *file = fopen(FRIEND_REQUEST_FILE, "r");
        if (!file) {
            perror("Failed to open friend requests file");
            return;
        }
        printf("read file\n");

        char line[256];
        while (fgets(line, sizeof(line), file)) {
            char *from_username = strtok(line, ":");
            char *to_username = strtok(NULL, "\n");
            if (from_username && strcmp(to_username, client->username) == 0) {
                Message friend_request_msg = create_message(MSG_FRIEND_REQUEST_LIST, (uint8_t *)from_username, strlen(from_username));
                send_message(client_socket, &friend_request_msg);
            }
        }

        fclose(file);
    }
}

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
            case MSG_PRIVATE_MSG: {
                // payload định dạng: <username_nhan>:<noi_dung>
                char receiver_username[128], message_content[512];
                sscanf((char *)message.payload, "%127[^:]:%511s", receiver_username, message_content);

                // Tìm người nhận trong danh sách online_clients
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (online_clients[i] && strcmp(online_clients[i]->username, receiver_username) == 0) {
                        // Gửi tin nhắn cho người nhận
                        Message msg = create_message(MSG_PRIVATE_MSG, (uint8_t *)message_content, strlen(message_content));
                        send_message(online_clients[i]->socket, &msg);
                        break;
                    }
                }
                break;
            }
            case MSG_FRIEND_REQUEST: {
                // payload định dạng: <username_nhan>
                char friend_username[128];
                sscanf((char *)message.payload, "%127s", friend_username);

                // Tìm người nhận trong danh sách online_clients
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (online_clients[i] && strcmp(online_clients[i]->username, friend_username) == 0) {
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
    if (client->is_logged_in) {
        remove_online_client(client);

        // Notify all clients of the updated online user list
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (online_clients[i] && online_clients[i]->is_logged_in) {
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
int main() {
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
