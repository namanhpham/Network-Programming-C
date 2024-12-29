#include <gtk/gtk.h>

GtkWidget *create_group_tab()
{
    GtkWidget *group_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    // Add group tab specific widgets here
    return group_vbox;
}
