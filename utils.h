#ifndef UTILS_H
#define UTILS_H

#include <libpq-fe.h>

#define MAX_LINE_LENGTH 256
void load_env_file(const char *filename);
void execute_sql(PGconn *conn, const char *sql);
void create_or_update_table(PGconn *conn, const char *table_name, const char *create_table_sql);
void init_db(PGconn *conn);
PGresult* get_user_by_username(PGconn *conn, const char *username);

#endif // UTILS_H