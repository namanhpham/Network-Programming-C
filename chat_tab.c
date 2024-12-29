#include <gtk/gtk.h>
#include "client_gui.h"

// Declare external variables
extern GtkWidget *chat_text_view;
extern GtkWidget *message_entry;

// Declare external function
extern void send_message_to_user(GtkWidget *widget, gpointer data);

GtkWidget *create_chat_tab()
{
    GtkWidget *chat_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    // Chat text view with scroll
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, -1, 300); // Fixed height for chat area

    chat_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_text_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), chat_text_view);
    gtk_box_pack_start(GTK_BOX(chat_vbox), scrolled_window, TRUE, TRUE, 0);

    GtkWidget *input_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(chat_vbox), input_hbox, FALSE, FALSE, 0);

    message_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(input_hbox), message_entry, TRUE, TRUE, 0);

    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(input_hbox), send_button, FALSE, FALSE, 0);

    g_signal_connect(send_button, "clicked", G_CALLBACK(send_message_to_user), NULL);

    return chat_vbox;
}
