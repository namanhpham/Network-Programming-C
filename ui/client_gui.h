#ifndef CLIENT_GUI_H
#define CLIENT_GUI_H

#include <gtk/gtk.h>
#include <pthread.h>
#include "../protocol.h"
#define CHUNK_SIZE 512
// Declare external variables
extern GtkWidget *chat_text_view;
extern GtkWidget *message_entry;

// Function declarations
void display_message(const char *message);
void *receive_messages(void *arg);
void connect_to_server();
void on_login_button_clicked(GtkWidget *widget, gpointer data);
GtkWidget *create_login_window();
GtkWidget *create_chat_window();

#endif // CLIENT_GUI_H
