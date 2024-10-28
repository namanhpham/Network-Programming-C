#include "protocol.h"
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>

// Hàm tạo thông điệp
Message create_message(uint8_t type, const uint8_t *payload, size_t payload_size) {
    Message message;
    message.type = type;
    
    // Sao chép payload vào thông điệp, giới hạn kích thước payload tối đa là 256 byte
    if (payload_size > sizeof(message.payload)) {
        payload_size = sizeof(message.payload); // Giới hạn kích thước nếu vượt quá
    }
    memcpy(message.payload, payload, payload_size);

    return message;
}

// Hàm gửi thông điệp qua socket
ssize_t send_message(int sockfd, const Message *message) {
    uint8_t buffer[sizeof(Message)];
    buffer[0] = message->type; // Byte đầu tiên là loại thông điệp

    // Sao chép payload vào buffer
    memcpy(buffer + 1, message->payload, sizeof(message->payload));

    // Gửi toàn bộ buffer qua socket
    ssize_t bytes_sent = send(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_sent < 0) {
        perror("Send failed");
    }
    return bytes_sent;
}

// Hàm nhận thông điệp từ socket
ssize_t receive_message(int sockfd, Message *message) {
    uint8_t buffer[sizeof(Message)];

    // Nhận toàn bộ buffer từ socket
    ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_received < 0) {
        perror("Receive failed");
        return bytes_received;
    }

    // Giải mã thông điệp
    message->type = buffer[0]; // Byte đầu tiên là loại thông điệp
    memcpy(message->payload, buffer + 1, sizeof(message->payload));

    return bytes_received;
}

// Hàm gửi tin nhắn thử nghiệm
void send_message_example(int sockfd) {
    uint8_t payload[] = "Hello, this is a test message!";
    Message msg = create_message(MSG_PRIVATE_MSG, payload, strlen((char *)payload));
    if (send_message(sockfd, &msg) < 0) {
        perror("Send message failed");
    }
}

