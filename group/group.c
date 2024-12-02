#include "group.h"
#include "../common.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Group *groups[MAX_CLIENTS] = {0}; // Define groups here

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
