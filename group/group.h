#ifndef GROUP_H
#define GROUP_H

#include "../protocol.h" // File này sẽ chứa hàm send_message, create_message, v.v. nếu cần
#include "../common.h"

typedef struct {
    char name[128];
    Client *members[MAX_CLIENTS];
} Group;

extern Group *groups[MAX_CLIENTS];

void handle_join_group(Client *client, const char *group_name);
void handle_create_group(Client *client, const char *group_name);
void handle_group_message(Client *client, const char *group_name, const char *message);

#endif
