#include <gtk/gtk.h>
#include "client_gui.h"

GtkWidget *create_friend_tab()
{
    GtkWidget *friend_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    // Add friend tab specific widgets here
    return friend_vbox;
}
