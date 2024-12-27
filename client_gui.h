#ifndef CLIENT_GUI_H
#define CLIENT_GUI_H

#include <gtk/gtk.h>
#include "protocol.h"

extern GtkWidget *window;
extern GtkWidget *login_button, *register_button, *logout_button;
extern GtkWidget *username_entry, *password_entry;
extern GtkWidget *status_label;
extern GtkWidget *main_grid;
extern GtkWidget *home_label;

void show_home_interface();
void hide_home_interface();
void on_login_button_clicked(GtkWidget *widget, gpointer data);
void on_register_button_clicked(GtkWidget *widget, gpointer data);
void on_logout_button_clicked(GtkWidget *widget, gpointer data);
void create_gui(int *sockfd);
void logout_user(int sockfd);

#endif // CLIENT_GUI_H
