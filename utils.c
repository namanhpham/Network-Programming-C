#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

void load_env_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening .env file");
        return;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file))
    {
        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // Skip empty lines or comments
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        // Split the line into key and value
        char *delimiter = strchr(line, '=');
        if (!delimiter)
        {
            fprintf(stderr, "Invalid line in .env file: %s\n", line);
            continue;
        }

        *delimiter = '\0'; // Replace '=' with '\0' to split key and value
        char *key = line;
        char *value = delimiter + 1;

        // Set the environment variable
        if (setenv(key, value, 1) != 0)
        {
            perror("Error setting environment variable");
        }
    }

    fclose(file);
}

void execute_sql(PGconn *conn, const char *sql)
{
    PGresult *res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "SQL command failed: %s\nError: %s\n", sql, PQerrorMessage(conn));
    }
    else
    {
        printf("SQL executed successfully\n\n");
    }
    PQclear(res);
}

void create_or_update_table(PGconn *conn, const char *table_name, const char *create_table_sql)
{
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT EXISTS (SELECT FROM pg_tables WHERE schemaname = 'public' AND tablename = '%s');",
             table_name);

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Failed to check table existence: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return;
    }

    int exists = strcmp(PQgetvalue(res, 0, 0), "t") == 0;
    PQclear(res);

    if (exists)
    {
        printf("Table '%s' already exists. Skipping creation.\n", table_name);
    }
    else
    {
        printf("Table '%s' does not exist. Creating it.\n", table_name);
        execute_sql(conn, create_table_sql);
    }
}
void update_table(PGconn *conn, const char *table_name, const char *create_table_sql)
{
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT EXISTS (SELECT FROM pg_tables WHERE schemaname = 'public' AND tablename = '%s');",
             table_name);

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Failed to check table existence: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return;
    }

    int exists = strcmp(PQgetvalue(res, 0, 0), "t") == 0;
    PQclear(res);

    if (exists)
    {
        printf("Table '%s' already exists. Updating it.\n", table_name);
        execute_sql(conn, create_table_sql);
    }
    else
    {
        printf("Table '%s' does not exist. Skip updating.\n", table_name);
    }
}


void init_db(PGconn *conn)
{

    // Define SQL commands for table creation
    const char *user_table_create =
        "CREATE TABLE IF NOT EXISTS users ("
        "id UUID PRIMARY KEY DEFAULT gen_random_uuid(), "
        "name TEXT NOT NULL, "
        "password TEXT NOT NULL, " // Add password column
        "last_online_at TIMESTAMP, "
        "last_offline_at TIMESTAMP, "
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP);";

    const char *group_table_create =
        "CREATE TABLE IF NOT EXISTS group_table ("
        "id UUID PRIMARY KEY DEFAULT gen_random_uuid(), "
        "name TEXT NOT NULL, "
        "creator_user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE, "
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP);";

    const char *group_members_table_create =
        "CREATE TABLE IF NOT EXISTS group_members ("
        "user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE, "
        "group_id UUID NOT NULL REFERENCES group_table(id) ON DELETE CASCADE, "
        "PRIMARY KEY (user_id, group_id));";

    const char *group_members_table_alter =
        "ALTER TABLE group_members ADD COLUMN IF NOT EXISTS deleted_at TIMESTAMP DEFAULT NULL;";

    const char *message_table_create =
        "CREATE TABLE IF NOT EXISTS message ("
        "id UUID PRIMARY KEY DEFAULT gen_random_uuid(), "
        "content TEXT NOT NULL, "
        "user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE, "
        "group_id UUID REFERENCES group_table(id) ON DELETE SET NULL, "
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP);";

    const char *friendship_table_create =
        "CREATE TABLE IF NOT EXISTS friendship ("
        "friend_requested_user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE, "
        "user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE, "
        "accepted_at TIMESTAMP, "
        "created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
        "PRIMARY KEY (user_id, friend_requested_user_id));";

    // Create tables
    create_or_update_table(conn, "users", user_table_create);
    create_or_update_table(conn, "group_table", group_table_create);
    create_or_update_table(conn, "message", message_table_create);
    create_or_update_table(conn, "friendship", friendship_table_create);
    create_or_update_table(conn, "group_members", group_members_table_create);
    update_table(conn, "group_members", group_members_table_alter);
}