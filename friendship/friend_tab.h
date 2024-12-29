#ifndef FRIEND_TAB_H
#define FRIEND_TAB_H

#include <gtk/gtk.h>

// Externally accessible widgets
extern GtkWidget *friends_list_box_friend_tab;
extern GtkWidget *friend_requests_box;
extern GtkWidget *search_entry;
extern GtkWidget *friend_tab_stack;

// Externally accessible functions
void on_send_friend_request_clicked(GtkWidget *widget, gpointer data);
void on_remove_friend_clicked(GtkWidget *button, gpointer friend_name);
void on_accept_friend_request_clicked(GtkWidget *button, gpointer friend_name);
void on_decline_friend_request_clicked(GtkWidget *button, gpointer friend_name);

// Populate functions for friends and requests
void populate_friends_list(const char *friends);
void populate_friend_requests(const char *requests);

// Page switching function
void on_switch_page_clicked(GtkWidget *button, gpointer data);

// Function to create the friend tab
GtkWidget *create_friend_tab();

#endif // FRIEND_TAB_H
