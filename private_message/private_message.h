
#ifndef PRIVATE_MESSAGE_H
#define PRIVATE_MESSAGE_H

#include "../protocol.h"
#include "../common.h"
#include "../utils.h"

void handle_private_message(Client *client, char *payload, PGconn *conn);

#endif // PRIVATE_MESSAGE_H