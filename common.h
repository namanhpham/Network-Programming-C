#ifndef COMMON_H
#define COMMON_H

#include "protocol.h" // Include nếu kiểu Client được định nghĩa ở đây, hoặc định nghĩa Client nếu chưa có.

#define MAX_CLIENTS 10

// Khai báo các mảng clients.
extern Client *clients[MAX_CLIENTS];
extern Client *online_clients[MAX_CLIENTS];

#endif // COMMON_H
