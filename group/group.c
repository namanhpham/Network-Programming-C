#include "group.h"
#include "../common.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Group *groups[MAX_CLIENTS] = {0}; // Define groups here

void handle_group_message(Client *client, const char *group_name, const char *message)
{
    Group *group = get_group_by_name(client->conn, group_name);
    if (group)
    {
        // Add message to the database
        add_group_message_record(client->conn, group->id, client->user_id, message);

        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (group->members[j] && group->members[j] != client)
            {
                const char *user_name = client->username;
                // Reallocate buffer if necessary
                size_t new_size = strlen(message) + strlen(user_name) + strlen(group_name) + 7;
                char *new_message = (char *)malloc(new_size);
                strcat(new_message, "[");
                strcat(new_message, group_name);
                strcat(new_message, "] ");
                strcat(new_message, user_name);
                strcat(new_message, ": ");
                strcat(new_message, message);
                Message msg = create_message(MSG_GROUP_MSG, (uint8_t *)new_message, strlen(new_message));
                send_message(group->members[j]->socket, &msg);
            }
        }
    }
    else
    {
        printf("Group '%s' not found.\n", group_name);
    }
}

void handle_create_group(Client *client, const char *group_name)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (groups[i] == NULL)
        {
            groups[i] = malloc(sizeof(Group));
            strncpy(groups[i]->name, group_name, sizeof(groups[i]->name));
            groups[i]->members[0] = client;
            printf("Group '%s' created by %s\n", group_name, client->username);
            return;
        }
    }
}

Group *get_group_by_name(PGconn *conn, const char *group_name)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (groups[i] && strcmp(groups[i]->name, group_name) == 0)
        {
            return groups[i];
        }
    }

    // If not found in memory, check the database
    Group *group = get_group_record(conn, group_name);
    if (group)
    {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (groups[i] == NULL)
            {
                groups[i] = group;
                return group;
            }
        }
    }

    return NULL;
}

void handle_join_group(Client *client, const char *group_name)
{
    Group *group = get_group_by_name(client->conn, group_name);
    if (group)
    {
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (group->members[j] == NULL)
            {
                group->members[j] = client;
                printf("%s joined group '%s'\n", client->username, group_name);

                // Add member to the database
                add_group_member_record(client->conn, group->id, client->user_id);

                return;
            }
        }
    }
    else
    {
        printf("Group '%s' not found.\n", group_name);
    }
}

void add_group_record(PGconn *conn, const char *group_name, const char *creator_user_id)
{
    const char *paramValues[2] = {group_name, creator_user_id};
    const char *sql = "INSERT INTO group_table (name, creator_user_id) VALUES ($1, $2);";

    PGresult *res = PQexecParams(conn, sql, 2, NULL, paramValues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Failed to add group record: %s\n", PQerrorMessage(conn));
    }
    else
    {
        printf("Group '%s' added successfully.\n", group_name);
    }
    PQclear(res);
}

Group *get_group_record(PGconn *conn, const char *group_name)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT id, name, creator_user_id, created_at FROM group_table WHERE name = '%s'", group_name);

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Failed to get group record: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }

    if (PQntuples(res) == 0)
    {
        printf("Group '%s' not found.\n", group_name);
        PQclear(res);
        return NULL;
    }

    Group *group = malloc(sizeof(Group));
    strncpy(group->id, PQgetvalue(res, 0, 0), sizeof(group->id));
    strncpy(group->name, PQgetvalue(res, 0, 1), sizeof(group->name));
    strncpy(group->creator_user_id, PQgetvalue(res, 0, 2), sizeof(group->creator_user_id));
    strncpy(group->created_at, PQgetvalue(res, 0, 3), sizeof(group->created_at));

    PQclear(res);

    // Initialize members to NULL
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        group->members[i] = NULL;
    }

    // Query to get group members
    snprintf(query, sizeof(query), "SELECT user_id FROM group_members WHERE group_id = '%s' AND deleted_at IS NULL", group->id);
    res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Failed to get group members: %s\n", PQerrorMessage(conn));
        PQclear(res);
        free(group);
        return NULL;
    }

    int num_members = PQntuples(res);
    for (int i = 0; i < num_members && i < MAX_CLIENTS; i++)
    {
        const char *user_id = PQgetvalue(res, i, 0);
        // Find the client by user_id and add to group->members
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (online_clients[j] && strcmp(online_clients[j]->user_id, user_id) == 0)
            {
                group->members[i] = online_clients[j];
                break;
            }
        }
    }

    PQclear(res);

    return group;
}

void add_group_member_record(PGconn *conn, const char *group_id, const char *user_id)
{
    const char *paramValues[2] = {group_id, user_id};

    // Check if the record exists and handle accordingly
    const char *check_sql = "SELECT deleted_at FROM group_members WHERE group_id = $1 AND user_id = $2;";
    PGresult *check_res = PQexecParams(conn, check_sql, 2, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(check_res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Failed to check group member record: %s\n", PQerrorMessage(conn));
        PQclear(check_res);
        return;
    }

    if (PQntuples(check_res) > 0) // Record exists
    {
        const char *deleted_at = PQgetvalue(check_res, 0, 0);
        if (deleted_at != NULL && strlen(deleted_at) > 0) // `deleted_at` is not NULL
        {
            const char *update_sql = "UPDATE group_members SET deleted_at = NULL WHERE group_id = $1 AND user_id = $2;";
            PGresult *update_res = PQexecParams(conn, update_sql, 2, NULL, paramValues, NULL, NULL, 0);

            if (PQresultStatus(update_res) != PGRES_COMMAND_OK)
            {
                fprintf(stderr, "Failed to update group member record: %s\n", PQerrorMessage(conn));
            }
            else
            {
                printf("User '%s' re-added to group '%s' by setting deleted_at to NULL.\n", user_id, group_id);
            }
            PQclear(update_res);
        }
        else
        {
            printf("User '%s' already exists in group '%s' and is active.\n", user_id, group_id);
        }
    }
    else // Record does not exist
    {
        const char *insert_sql = "INSERT INTO group_members (group_id, user_id) VALUES ($1, $2);";
        PGresult *insert_res = PQexecParams(conn, insert_sql, 2, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(insert_res) != PGRES_COMMAND_OK)
        {
            fprintf(stderr, "Failed to add group member record: %s\n", PQerrorMessage(conn));
        }
        else
        {
            printf("User '%s' added to group '%s' successfully.\n", user_id, group_id);
        }
        PQclear(insert_res);
    }
    PQclear(check_res);
}

void add_group_message_record(PGconn *conn, const char *group_id, const char *user_id, const char *message)
{
    const char *paramValues[3] = {group_id, user_id, message};
    const char *sql = "INSERT INTO message (group_id, user_id, content) VALUES ($1, $2, $3);";

    PGresult *res = PQexecParams(conn, sql, 3, NULL, paramValues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Failed to add group message record: %s\n", PQerrorMessage(conn));
    }
    else
    {
        printf("Message added to group '%s' by user '%s' successfully.\n", group_id, user_id);
    }
    PQclear(res);
}

void handle_list_groups(Client *client)
{
    const char *sql = "SELECT name FROM group_table;";
    PGresult *res = PQexec(client->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Failed to list groups: %s\n", PQerrorMessage(client->conn));
        PQclear(res);
        return;
    }

    char group_list[1024] = "";
    for (int i = 0; i < PQntuples(res); i++)
    {
        strcat(group_list, PQgetvalue(res, i, 0));
        strcat(group_list, "\n");
    }
    PQclear(res);

    Message msg = create_message(MSG_LIST_GROUPS, (uint8_t *)group_list, strlen(group_list));
    send_message(client->socket, &msg);
}

void handle_see_group_messages(Client *client, const char *group_name)
{
    Group *group = get_group_by_name(client->conn, group_name);
    if (group)
    {
        const char *paramValues[1] = {group->id};
        const char *sql = "SELECT u.name, m.content "
                          "FROM message m "
                          "JOIN users u ON u.id = m.user_id "
                          "WHERE m.group_id = $1 "
                          "ORDER BY m.created_at;";

        PGresult *res = PQexecParams(client->conn, sql, 1, NULL, paramValues, NULL, NULL, 0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
            fprintf(stderr, "Failed to get group messages: %s\n", PQerrorMessage(client->conn));
            PQclear(res);
            return;
        }

        char chunk[CHUNK_SIZE];
        snprintf(chunk, CHUNK_SIZE, "Message history for group: %s\n", group_name);
        Message initial_msg = create_message(MSG_GROUP_MSG_HISTORY, (uint8_t *)chunk, strlen(chunk));
        send_message(client->socket, &initial_msg);

        memset(chunk, 0, CHUNK_SIZE); // Clear the buffer for reuse

        for (int i = 0; i < PQntuples(res); i++)
        {
            const char *user_name = PQgetvalue(res, i, 0);
            const char *message_content = PQgetvalue(res, i, 1);

            // Format the message into the chunk
            snprintf(chunk, CHUNK_SIZE, "%s: %s\n", user_name, message_content);

            // Send the chunk
            Message msg = create_message(MSG_GROUP_MSG_HISTORY, (uint8_t *)chunk, strlen(chunk));
            send_message(client->socket, &msg);

            memset(chunk, 0, CHUNK_SIZE); // Clear the buffer after each send
        }

        PQclear(res);

        Message end_msg = create_message(MSG_GROUP_MSG_HISTORY, (uint8_t *)chunk, strlen(chunk));
        send_message(client->socket, &end_msg);
    }
    else
    {
        printf("Group '%s' not found.\n", group_name);
    }
}
void set_group_member_deleted_at(PGconn *conn, const char *group_id, const char *user_id)
{
    const char *paramValues[2] = {group_id, user_id};
    const char *sql = "UPDATE group_members SET deleted_at = NOW() WHERE group_id = $1 AND user_id = $2 AND deleted_at IS NULL;";

    PGresult *res = PQexecParams(conn, sql, 2, NULL, paramValues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Failed to set deleted_at for group member: %s\n", PQerrorMessage(conn));
    }
    else
    {
        printf("User '%s' marked as removed from group '%s' successfully.\n", user_id, group_id);
    }
    PQclear(res);
}
void handle_leave_group(Client *client, const char *group_name)
{
    Group *group = get_group_by_name(client->conn, group_name);
    if (group)
    {
        set_group_member_deleted_at(client->conn, group->id, client->user_id);
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (group->members[j])
            {
                const char *user_name = client->username;
                char message[1024] = "";
                snprintf(message, sizeof(message), "%s left group '%s'\n", client->username, group_name);
                size_t new_size = strlen(message) + strlen(user_name) + strlen(group_name) + 7;
                char *new_message = (char *)malloc(new_size);
                strcat(new_message, "[");
                strcat(new_message, group_name);
                strcat(new_message, "] ");
                strcat(new_message, user_name);
                strcat(new_message, ": ");
                strcat(new_message, message);
                Message msg = create_message(MSG_GROUP_MSG, (uint8_t *)new_message, strlen(new_message));
                send_message(group->members[j]->socket, &msg);
            }
        }
    }
    else
    {
        printf("Group '%s' not found.\n", group_name);
    }
}

void handle_remove_group_member(Client *client, const char *group_name, const char *member_username)
{
    Group *group = get_group_by_name(client->conn, group_name);
    if (group)
    {
        if (strcmp(group->creator_user_id, client->user_id) != 0)
        {
            char *message = "Only the group creator can remove members.\n";
            Message msg = create_message(RESP_REMOVE_GROUP_MEMBER, (uint8_t *)message, strlen(message));
            send_message(client->socket, &msg);
            return;
        }

        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (group->members[j])
            {
                Client *member = group->members[j];

                char message[1024] = "";
                snprintf(message, sizeof(message), "%s removed from group '%s' by %s\n", member_username, group_name, client->username);
                Message msg = create_message(RESP_REMOVE_GROUP_MEMBER, (uint8_t *)message, strlen(message));
                send_message(group->members[j]->socket, &msg);
                if (strcmp(group->members[j]->username, member_username) == 0)
                {
                    group->members[j] = NULL;
                    set_group_member_deleted_at(client->conn, group->id, member->user_id);
                }
            }
        }
    }

    else
    {
        printf("Group '%s' not found.\n", group_name);
    }
}
