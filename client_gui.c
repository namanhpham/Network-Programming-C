#include <gtk/gtk.h>
#include "client_gui.h"
#include "protocol.h"

GtkWidget *window;
GtkWidget *login_button, *register_button, *logout_button;
GtkWidget *username_entry, *password_entry;
GtkWidget *status_label;
GtkWidget *main_grid;
GtkWidget *home_label;

void show_home_interface()
{
    gtk_widget_hide(login_button);
    gtk_widget_hide(register_button);
    gtk_widget_show(logout_button);
    gtk_widget_show(main_grid);
    gtk_label_set_text(GTK_LABEL(status_label), "Welcome to the home page!");
}

void hide_home_interface()
{
    gtk_widget_show(login_button);
    gtk_widget_show(register_button);
    gtk_widget_hide(logout_button);
    gtk_widget_hide(main_grid);
    gtk_label_set_text(GTK_LABEL(status_label), "Please log in or register.");
}

// Function to handle login button click
void on_login_button_clicked(GtkWidget *widget, gpointer data)
{
    int sockfd = *(int *)data;
    const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));

    char payload[256];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);

    Message msg = create_message(MSG_LOGIN, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        gtk_label_set_text(GTK_LABEL(status_label), "Login failed");
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(status_label), "Login request sent");
    }
}

// Function to handle register button click
void on_register_button_clicked(GtkWidget *widget, gpointer data)
{
    int sockfd = *(int *)data;
    const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));

    char payload[256];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);

    Message msg = create_message(MSG_REGISTER, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        gtk_label_set_text(GTK_LABEL(status_label), "Registration failed");
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(status_label), "Registration request sent");
    }
}

// Function to handle logout button click
void on_logout_button_clicked(GtkWidget *widget, gpointer data)
{
    int sockfd = *(int *)data;
    logout_user(sockfd);
    hide_home_interface();
    gtk_label_set_text(GTK_LABEL(status_label), "Logged out");
}

void logout_user(int sockfd)
{
    Message msg = create_message(MSG_LOGOUT, NULL, 0);
    send_message(sockfd, &msg);
}

void apply_css(GtkWidget *widget, GtkCssProvider *provider)
{
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    if (GTK_IS_CONTAINER(widget))
    {
        gtk_container_forall(GTK_CONTAINER(widget), (GtkCallback)apply_css, provider);
    }
}

void create_gui(int *sockfd)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
                                    "window {"
                                    "  background-color: #f0f0f0;"
                                    "}"
                                    "label {"
                                    "  font-size: 14px;"
                                    "}"
                                    "button {"
                                    "  background-color: #4CAF50;"
                                    "  color: white;"
                                    "  border: none;"
                                    "  padding: 10px 20px;"
                                    "  text-align: center;"
                                    "  text-decoration: none;"
                                    "  display: inline-block;"
                                    "  font-size: 16px;"
                                    "  margin: 4px 2px;"
                                    "  cursor: pointer;"
                                    "}"
                                    "button:hover {"
                                    "  background-color: #45a049;"
                                    "}",
                                    -1, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "App");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    GtkWidget *username_label = gtk_label_new("Username:");
    gtk_grid_attach(GTK_GRID(grid), username_label, 0, 0, 1, 1);
    username_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), username_entry, 1, 0, 1, 1);

    GtkWidget *password_label = gtk_label_new("Password:");
    gtk_grid_attach(GTK_GRID(grid), password_label, 0, 1, 1, 1);
    password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 1, 1, 1, 1);

    login_button = gtk_button_new_with_label("Login");
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), sockfd);
    gtk_grid_attach(GTK_GRID(grid), login_button, 0, 2, 1, 1);

    register_button = gtk_button_new_with_label("Register");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), sockfd);
    gtk_grid_attach(GTK_GRID(grid), register_button, 1, 2, 1, 1);

    logout_button = gtk_button_new_with_label("Logout");
    g_signal_connect(logout_button, "clicked", G_CALLBACK(on_logout_button_clicked), sockfd);
    gtk_grid_attach(GTK_GRID(grid), logout_button, 0, 3, 2, 1);
    gtk_widget_hide(logout_button); // Initially hide the logout button

    status_label = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 4, 2, 1);

    main_grid = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(grid), main_grid, 0, 5, 2, 1);
    gtk_widget_hide(main_grid); // Initially hide the main interface

    home_label = gtk_label_new("Home Page");
    gtk_grid_attach(GTK_GRID(main_grid), home_label, 0, 0, 1, 1);

    apply_css(window, provider);
    gtk_widget_show_all(window);
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    int sockfd;
    create_gui(&sockfd);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
