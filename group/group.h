#ifndef GROUP_H
#define GROUP_H

#include "../protocol.h" // File này sẽ chứa hàm send_message, create_message, v.v. nếu cần
#include "../common.h"

typedef struct {
    char id[128];
    char name[128];
    char creator_user_id[128];
    char created_at[128];
    Client *members[MAX_CLIENTS];
} Group;

extern Group *groups[MAX_CLIENTS];

void handle_join_group(Client *client, const char *group_name);
void handle_create_group(Client *client, const char *group_name);
void handle_group_message(Client *client, const char *group_name, const char *message);
Group *get_group_by_name(PGconn *conn, const char *group_name);
void add_group_record(PGconn *conn, const char *group_name, const char *creator_user_id);
Group *get_group_record(PGconn *conn, const char *group_name);
void add_group_member_record(PGconn *conn, const char *group_id, const char *user_id);
void handle_list_groups(Client *client);
void handle_see_group_messages(Client *client, const char *group_name);
void add_group_message_record(PGconn *conn, const char *group_id, const char *user_id, const char *message);
void handle_leave_group(Client *client, const char *group_name);
void handle_remove_group_member(Client *client, const char *group_name, const char *member_username);
#endif
