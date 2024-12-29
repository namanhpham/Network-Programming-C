#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <sys/types.h> // For ssize_t
#include <stddef.h>    // For size_t
#include <netinet/in.h>
#include <libpq-fe.h>

// Loại thông điệp (Message Types)
#define MSG_REGISTER 0x01                // Đăng ký tài khoản
#define MSG_LOGIN 0x02                   // Đăng nhập
#define MSG_LOGOUT 0x03                  // Đăng xuất
#define MSG_FRIEND_REQUEST 0x04          // Gửi lời mời kết bạn
#define MSG_FRIEND_ACCEPT 0x05           // Chấp nhận lời mời kết bạn
#define MSG_FRIEND_DECLINE 0x06          // Từ chối lời mời kết bạn
#define MSG_FRIEND_REMOVE 0x07           // Hủy kết bạn
#define MSG_FRIEND_LIST 0x08             // Lấy danh sách bạn bè và trạng thái
#define MSG_PRIVATE_MSG 0x09             // Gửi tin nhắn riêng tư
#define MSG_DISCONNECT 0x0A              // Ngắt kết nối
#define MSG_CREATE_GROUP 0x0B            // Tạo nhóm chat
#define MSG_ADD_GROUP_MEMBER 0x0C        // Thêm thành viên vào nhóm chat
#define MSG_REMOVE_GROUP_MEMBER 0x0D     // Xóa thành viên khỏi nhóm chat
#define MSG_LEAVE_GROUP 0x0E             // Rời nhóm chat
#define MSG_GROUP_MSG 0x0F               // Gửi tin nhắn trong nhóm chat
#define MSG_OFFLINE_MSG 0x10             // Gửi tin nhắn offline
#define MSG_LOG_ACTIVITY 0x11            // Ghi log hoạt động
#define MSG_STATUS_UPDATE 0x12           // Cập nhật trạng thái online/offline
#define MSG_ONLINE_USERS 0x13            // Danh sách người dùng đang online
#define MSG_FRIEND_REQUEST_ACCEPTED 0x14 // Thông báo chấp nhận lời mời kết bạn
#define MSG_FRIEND_REQUEST_DECLINED 0x15 // Thông báo từ chối lời mời kết bạn
#define MSG_FRIEND_REQUEST_LIST 0x16     // Danh sách lời mời kết bạn
#define MSG_FRIEND_REMOVED 0x17          // Thông báo hủy kết bạn
#define MSG_FRIENDS_LIST 0x18            // Danh sách bạn bè
#define MSG_GROUP_MSG_HISTORY 0x19       // Lịch sử chat trong group
#define MSG_JOIN_GROUP 0x30              // Tham gia nhóm chat.
#define MSG_LIST_GROUPS 0x31             // Lấy danh sách các nhóm chat.
#define MSG_PRIVATE_MSG_HISTORY 0x32     // Lịch sử tin nhắn riêng tư
#define MSG_SEE_JOINED_GROUPS 0x35       // Xem các nhóm đã tham gia
// Mã phản hồi (Response Codes)
#define RESP_LOGIN_SUCCESS 0x20    // Thông điệp thành công
#define RESP_LOGIN_FAILURE 0x21    // Thông điệp thất bại
#define RESP_FRIEND_LIST 0x22      // Danh sách bạn bè và trạng thái
#define RESP_REGISTER_SUCCESS 0x23 // Đăng ký tài khoản thành công
#define RESP_SUCCESS 0x24          // Thành công
#define RESP_FAILURE 0x25          // Thất bại

#define RESP_LEAVE_GROUP 0x33         // Thông báo rời nhóm chat
#define RESP_REMOVE_GROUP_MEMBER 0x34 // Thông báo xóa thành viên khỏi nhóm chat
#define RESP_SEE_JOINED_GROUPS 0x36   // Xem các nhóm đã tham gia
// Định dạng thông điệp
// Message Type (1 byte) | Payload (tùy loại thông điệp)

// Ví dụ Định dạng Payload cho từng loại thông điệp:
// 1. Đăng ký tài khoản: MSG_REGISTER
//    Payload: [username (var)] [password (var)]
// 2. Đăng nhập: MSG_LOGIN
//    Payload: [username (var)] [password (var)]
// 3. Đăng xuất: MSG_LOGOUT
//    Payload: [user_id (4 byte)]
// 4. Lời mời kết bạn: MSG_FRIEND_REQUEST
//    Payload: [from_user_id (4 byte)] [to_user_id (4 byte)]
// 5. Chấp nhận lời mời kết bạn: MSG_FRIEND_ACCEPT
//    Payload: [from_user_id (4 byte)] [to_user_id (4 byte)]
// 6. Từ chối lời mời kết bạn: MSG_FRIEND_DECLINE
//    Payload: [from_user_id (4 byte)] [to_user_id (4 byte)]
// 7. Hủy kết bạn: MSG_FRIEND_REMOVE
//    Payload: [user_id (4 byte)] [friend_user_id (4 byte)]
// 8. Lấy danh sách bạn bè: MSG_FRIEND_LIST
//    Payload: [user_id (4 byte)]
// 9. Tin nhắn riêng tư: MSG_PRIVATE_MSG
//    Payload: [from_user_id (4 byte)] [to_user_id (4 byte)] [message (var)]
// 10. Ngắt kết nối: MSG_DISCONNECT
//     Payload: [user_id (4 byte)]
// 11. Tạo nhóm chat: MSG_CREATE_GROUP
//     Payload: [group_name (var)] [user_id (4 byte)]
// 12. Thêm thành viên vào nhóm: MSG_ADD_GROUP_MEMBER
//     Payload: [group_id (4 byte)] [user_id (4 byte)]
// 13. Xóa thành viên khỏi nhóm: MSG_REMOVE_GROUP_MEMBER
//     Payload: [group_id (4 byte)] [user_id (4 byte)]
// 14. Rời nhóm: MSG_LEAVE_GROUP
//     Payload: [group_id (4 byte)] [user_id (4 byte)]
// 15. Tin nhắn nhóm: MSG_GROUP_MSG
//     Payload: [from_user_id (4 byte)] [group_id (4 byte)] [message (var)]
// 16. Tin nhắn offline: MSG_OFFLINE_MSG
//     Payload: [to_user_id (4 byte)] [message (var)]
// 17. Log hoạt động: MSG_LOG_ACTIVITY
//     Payload: [user_id (4 byte)] [activity (var)]
// 18. Cập nhật trạng thái: MSG_STATUS_UPDATE
//     Payload: [user_id (4 byte)] [status (1 byte)]

// Hàm để mã hóa thông điệp (Tạo cấu trúc)
typedef struct
{
    uint8_t type;         // Loại thông điệp (Message Type)
    uint8_t payload[256]; // Dữ liệu của thông điệp (tối đa 256 bytes)
} Message;

// Struct lưu thông tin cho mỗi client
typedef struct
{
    int socket;
    struct sockaddr_in address;
    char user_id[37]; // UUID as identifier
    char username[128];
    int is_logged_in;
    PGconn *conn;
} Client;

// Hàm để tạo thông điệp
Message create_message(uint8_t type, const uint8_t *payload, size_t payload_size);

// Hàm gửi thông điệp qua socket
ssize_t send_message(int sockfd, const Message *message);

// Hàm nhận thông điệp từ socket
ssize_t receive_message(int sockfd, Message *message);

#endif // PROTOCOL_H
