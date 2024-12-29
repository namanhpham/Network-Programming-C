
#ifndef GROUP_TAB_H
#define GROUP_TAB_H

#include <gtk/gtk.h>
#include "../protocol.h"

GtkWidget *create_group_tab();
void update_group_list(const char *groups);
void update_message_history(const char *message);


#endif // GROUP_TAB_H