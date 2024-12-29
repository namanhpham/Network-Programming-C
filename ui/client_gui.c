#include <gtk/gtk.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "../protocol.h"
#include "client_gui.h"
#include "../private_message/chat_tab.h"
#include "../friendship/friend_tab.h"
#include "../group/group_tab.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

static int sockfd;
static char *current_friends_list = NULL;
static pthread_t recv_thread;
static GtkWidget *login_window;
static GtkWidget *register_window;
static GtkWidget *chat_window;
GtkWidget *message_entry;  // Remove static
GtkWidget *chat_text_view; // Remove static
static GtkWidget *recipient_entry;
static int is_logged_in = 0;

static GtkWidget *chat_vbox;
static GtkWidget *friend_vbox;
static GtkWidget *group_vbox;
static GtkWidget *current_vbox;

static GtkWidget *chat_sidebar;
static GtkWidget *friend_sidebar;
static GtkWidget *group_sidebar;
static GtkWidget *friends_list_box;

// Function to display a message in the chat window
void display_message(const char *message)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
    GtkTextIter end;

    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, message, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
}

// Function to display a message in the chat window with sender's name
void display_message_with_sender(const char *sender, const char *message)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
    GtkTextIter end;

    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, sender, -1);
    gtk_text_buffer_insert(buffer, &end, ": ", -1);
    gtk_text_buffer_insert(buffer, &end, message, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
}

// Function to clear the chat window
void clear_chat_window()
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
    gtk_text_buffer_set_text(buffer, "", -1);
}

void on_friend_button_clicked(GtkWidget *widget, gpointer data)
{
    const char *friend_name = (const char *)data;

    g_print("Button clicked for friend: %s\n", friend_name);

    gtk_entry_set_text(GTK_ENTRY(recipient_entry), friend_name);
}

void display_friends_list(const char *friends)
{
    // So sánh với danh sách bạn bè hiện tại
    if (current_friends_list && strcmp(current_friends_list, friends) == 0)
    {
        g_print("Friends list unchanged, skipping update.\n");
        return; // Không cần cập nhật
    }

    // Cập nhật danh sách bạn bè hiện tại
    if (current_friends_list)
    {
        free(current_friends_list);
    }
    current_friends_list = strdup(friends);

    // Xóa toàn bộ các widget con trong friends_list_boxs
    gtk_container_foreach(GTK_CONTAINER(friends_list_box), (GtkCallback)gtk_widget_destroy, NULL);

    char *friends_copy = strdup(friends);
    char *friend = strtok(friends_copy, "\n");
    while (friend != NULL)
    {
        if (strncmp(friend, "Friend numbers:", 15) != 0)
        {
            GtkWidget *button = gtk_button_new_with_label(friend);

            // Kết nối tín hiệu clicked cho nút
            g_signal_connect(button, "clicked", G_CALLBACK(on_friend_button_clicked), g_strdup(friend));
            g_print("Connected signal for friend: %s\n", friend);

            gtk_box_pack_start(GTK_BOX(friends_list_box), button, FALSE, FALSE, 0);
        }
        friend = strtok(NULL, "\n");
    }
    free(friends_copy);

    gtk_widget_show_all(friends_list_box);
}

// Function to handle received messages
void *receive_messages(void *arg)
{
    Message msg;

    while (1)
    {
        if (receive_message(sockfd, &msg) <= 0)
        {
            g_print("Server disconnected\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        if (msg.type == RESP_LOGIN_SUCCESS || msg.type == RESP_REGISTER_SUCCESS)
        {
            is_logged_in = 1;
        }
        else if (msg.type == RESP_LOGIN_FAILURE)
        {
            is_logged_in = 0;
        }

        switch (msg.type)
        {
        case MSG_PRIVATE_MSG_HISTORY:
        case MSG_PRIVATE_MSG:
            g_idle_add((GSourceFunc)display_message, (gpointer)msg.payload);
            break;
        case MSG_LOGOUT:
            break;
        case MSG_FRIENDS_LIST:
        {
            char *friends_data = g_strdup((char *)msg.payload);

            g_idle_add((GSourceFunc)display_friends_list, friends_data);

            g_print("Friends list received: %s\n", friends_data);
            break;
        }

        default:
            g_idle_add((GSourceFunc)display_message, (gpointer)msg.payload);
            break;
        }
    }
    return NULL;
}

// Function to send messages to a specific user
void send_message_to_user(GtkWidget *widget, gpointer data)
{
    const char *recipient = gtk_entry_get_text(GTK_ENTRY(recipient_entry));
    const char *text = gtk_entry_get_text(GTK_ENTRY(message_entry));

    if (strlen(recipient) > 0 && strlen(text) > 0)
    {
        char payload[512];
        snprintf(payload, sizeof(payload), "%s:%s", recipient, text);

        Message msg = create_message(MSG_PRIVATE_MSG, (uint8_t *)payload, strlen(payload));
        if (send_message(sockfd, &msg) < 0)
        {
            g_print("Failed to send message\n");
        }

        gtk_entry_set_text(GTK_ENTRY(message_entry), "");
    }
    else
    {
        g_print("Recipient or message cannot be empty\n");
    }
}

// Function to request and display private message history
void see_private_messages(GtkWidget *widget, gpointer data)
{
    const char *friend_username = gtk_entry_get_text(GTK_ENTRY(recipient_entry));

    if (strlen(friend_username) > 0)
    {
        Message msg = create_message(MSG_PRIVATE_MSG_HISTORY, (uint8_t *)friend_username, strlen(friend_username));
        if (send_message(sockfd, &msg) < 0)
        {
            g_print("Failed to see private messages\n");
        }
    }
}

// Function to handle recipient entry change
void on_recipient_entry_changed(GtkWidget *widget, gpointer data)
{
    if (widget == recipient_entry)
    {
        clear_chat_window();
        const char *friend_username = gtk_entry_get_text(GTK_ENTRY(recipient_entry));
        if (strlen(friend_username) > 0)
        {
            Message msg = create_message(MSG_PRIVATE_MSG_HISTORY, (uint8_t *)friend_username, strlen(friend_username));
            if (send_message(sockfd, &msg) < 0)
            {
                g_print("Failed to see private messages\n");
            }
        }
    }
}

// Function to initialize the connection to the server
void connect_to_server()
{
    struct sockaddr_in server_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
}

// Function to handle login button click
void on_login_button_clicked(GtkWidget *widget, gpointer data)
{
    GtkEntry *username_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(widget), "username_entry"));
    GtkEntry *password_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(widget), "password_entry"));

    const char *username = gtk_entry_get_text(username_entry);
    const char *password = gtk_entry_get_text(password_entry);

    char payload[256];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);

    Message msg = create_message(MSG_LOGIN, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        g_print("Login failed\n");
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(login_window),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "Login failed. Please try again.");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
    }
    else
    {
        // Wait for server response
        struct timespec delay = {0, 500000000L}; // 500ms
        nanosleep(&delay, NULL);

        if (is_logged_in)
        {
            gtk_widget_hide(login_window);
            gtk_widget_show_all(chat_window);

            Message friends_list_msg = create_message(MSG_FRIENDS_LIST, (uint8_t *)"", 0);
            if (send_message(sockfd, &friends_list_msg) < 0)
            {
                g_print("Failed to request friends list\n");
            }
        }
        else
        {
            GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(login_window),
                                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                                             GTK_MESSAGE_ERROR,
                                                             GTK_BUTTONS_CLOSE,
                                                             "Login failed. Please try again.");
            gtk_dialog_run(GTK_DIALOG(error_dialog));
            gtk_widget_destroy(error_dialog);
        }
    }
}

// Function to handle register button click
void on_register_button_clicked(GtkWidget *widget, gpointer data)
{
    GtkEntry *username_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(widget), "username_entry"));
    GtkEntry *password_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(widget), "password_entry"));

    const char *username = gtk_entry_get_text(username_entry);
    const char *password = gtk_entry_get_text(password_entry);

    char payload[256];
    snprintf(payload, sizeof(payload), "%s:%s", username, password);

    Message msg = create_message(MSG_REGISTER, (uint8_t *)payload, strlen(payload));
    if (send_message(sockfd, &msg) < 0)
    {
        g_print("Registration failed\n");
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(login_window),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "Registration failed. Please try again.");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
    }
    else
    {
        // Wait for server response
        struct timespec delay = {0, 500000000L}; // 500ms
        nanosleep(&delay, NULL);

        if (is_logged_in)
        {
            gtk_widget_hide(register_window);
            gtk_widget_show_all(chat_window);
        }
        else
        {
            GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(register_window),
                                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                                             GTK_MESSAGE_ERROR,
                                                             GTK_BUTTONS_CLOSE,
                                                             "Registration failed. Please try again.");
            gtk_dialog_run(GTK_DIALOG(error_dialog));
            gtk_widget_destroy(error_dialog);
        }
    }
}

// Function to handle logout button click
void on_logout_button_clicked(GtkWidget *widget, gpointer data)
{
    Message msg = create_message(MSG_LOGOUT, (uint8_t *)"Logout", 6);
    if (send_message(sockfd, &msg) < 0)
    {
        g_print("Logout failed\n");
    }
    else
    {
        clear_chat_window();
        gtk_widget_hide(chat_window);
        gtk_widget_show_all(login_window);
    }
}

// Function to handle application exit
void on_app_exit()
{
    if (current_friends_list)
    {
        free(current_friends_list);
    }

    Message msg = create_message(MSG_DISCONNECT, (uint8_t *)"Disconnecting", 12);
    if (send_message(sockfd, &msg) < 0)
    {
        g_print("Failed to send disconnect message\n");
    }
    close(sockfd);
}

// Function to switch tabs
void switch_tab(GtkWidget *widget, gpointer vbox)
{
    gtk_widget_hide(current_vbox);
    gtk_widget_show_all(GTK_WIDGET(vbox));
    current_vbox = GTK_WIDGET(vbox);

    // Reset all tab buttons to normal color
    GtkWidget *parent = gtk_widget_get_parent(widget);
    GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter))
    {
        GtkWidget *button = GTK_WIDGET(iter->data);
        gtk_widget_set_name(button, "tab_button");
    }
    g_list_free(children);

    // Highlight the active tab button
    gtk_widget_set_name(widget, "active_tab_button");

    // Update sidebar visibility
    gtk_widget_hide(chat_sidebar);
    gtk_widget_hide(friend_sidebar);
    gtk_widget_hide(group_sidebar);

    if (vbox == chat_vbox)
    {
        gtk_widget_show_all(chat_sidebar);
    }
    else if (vbox == friend_vbox)
    {
        gtk_widget_show_all(friend_sidebar);
    }
    else if (vbox == group_vbox)
    {
        gtk_widget_show_all(group_sidebar);
    }
}

// Function to create the login window
GtkWidget *create_login_window()
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Login");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);    // Add spacing between rows
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10); // Add spacing between columns
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(window), grid);

    GtkWidget *username_label = gtk_label_new("Username:");
    GtkWidget *username_entry = gtk_entry_new();
    GtkWidget *password_label = gtk_label_new("Password:");
    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    GtkWidget *login_button = gtk_button_new_with_label("Login");
    GtkWidget *register_button = gtk_button_new_with_label("Register");

    gtk_grid_attach(GTK_GRID(grid), username_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), username_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), login_button, 0, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), register_button, 0, 3, 2, 1);

    gtk_widget_set_halign(login_button, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(register_button, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(login_button, 100, -1);
    gtk_widget_set_size_request(register_button, 100, -1);

    g_object_set_data(G_OBJECT(login_button), "username_entry", username_entry);
    g_object_set_data(G_OBJECT(login_button), "password_entry", password_entry);

    g_object_set_data(G_OBJECT(register_button), "username_entry", username_entry);
    g_object_set_data(G_OBJECT(register_button), "password_entry", password_entry);

    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), NULL);
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    return window;
}

// Function to create the chat window
GtkWidget *create_chat_window()
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600); // Fixed width and height

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(window), hbox);

    // Tab bar with icons on the far left side
    GtkWidget *tab_bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(tab_bar, 50, -1);
    gtk_widget_set_name(tab_bar, "tab_bar"); // Set name for CSS styling
    gtk_box_pack_start(GTK_BOX(hbox), tab_bar, FALSE, FALSE, 0);

    GtkWidget *chat_icon = gtk_button_new_with_label("Chat");
    GtkWidget *friend_icon = gtk_button_new_with_label("Friend");
    GtkWidget *group_icon = gtk_button_new_with_label("Group");
    GtkWidget *logout_icon = gtk_button_new_with_label("Logout");

    gtk_box_pack_start(GTK_BOX(tab_bar), chat_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tab_bar), friend_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tab_bar), group_icon, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(tab_bar), logout_icon, FALSE, FALSE, 0);

    // Chat sidebar
    chat_sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(chat_sidebar, 150, -1);
    gtk_box_pack_start(GTK_BOX(hbox), chat_sidebar, FALSE, FALSE, 0);

    // Recipient bar at the top of the chat sidebar
    recipient_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(recipient_entry), "Recipient username");
    gtk_box_pack_start(GTK_BOX(chat_sidebar), recipient_entry, FALSE, FALSE, 0);
    g_signal_connect(recipient_entry, "changed", G_CALLBACK(on_recipient_entry_changed), NULL);

    // Friends list box
    friends_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(chat_sidebar), friends_list_box, TRUE, TRUE, 0);

    // Friend sidebar
    friend_sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(friend_sidebar, 150, -1);
    gtk_box_pack_start(GTK_BOX(hbox), friend_sidebar, FALSE, FALSE, 0);

    // Group sidebar
    group_sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(group_sidebar, 150, -1);
    gtk_box_pack_start(GTK_BOX(hbox), group_sidebar, FALSE, FALSE, 0);

    // Chat vbox
    chat_vbox = create_chat_tab();
    gtk_box_pack_start(GTK_BOX(hbox), chat_vbox, TRUE, TRUE, 0);

    // Friend vbox
    friend_vbox = create_friend_tab();
    gtk_box_pack_start(GTK_BOX(hbox), friend_vbox, TRUE, TRUE, 0);

    // Group vbox
    group_vbox = create_group_tab();
    gtk_box_pack_start(GTK_BOX(hbox), group_vbox, TRUE, TRUE, 0);

    // Initially show chat vbox and sidebar
    current_vbox = chat_vbox;
    gtk_widget_show_all(chat_vbox);
    gtk_widget_show_all(chat_sidebar);
    gtk_widget_hide(friend_vbox);
    gtk_widget_hide(friend_sidebar);
    gtk_widget_hide(group_vbox);
    gtk_widget_hide(group_sidebar);

    g_signal_connect(chat_icon, "clicked", G_CALLBACK(switch_tab), chat_vbox);
    g_signal_connect(friend_icon, "clicked", G_CALLBACK(switch_tab), friend_vbox);
    g_signal_connect(group_icon, "clicked", G_CALLBACK(switch_tab), group_vbox);
    g_signal_connect(logout_icon, "clicked", G_CALLBACK(on_logout_button_clicked), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    return window;
}

// Function to apply CSS styling
void apply_css()
{
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
                                    "* {"
                                    "  font-family: Arial, sans-serif;"
                                    "  font-size: 14px;"
                                    "}"
                                    "GtkButton {"
                                    "  background-color: #4CAF50;"
                                    "  color: white;"
                                    "  border-radius: 5px;"
                                    "  padding: 5px;"
                                    "}"
                                    "#active_tab_button {"
                                    "  background-color: #388E3C;"
                                    "}"
                                    "#tab_bar {"
                                    "  background-color: #333;"
                                    "  padding: 10px;"
                                    "}"
                                    "GtkEntry {"
                                    "  border: 1px solid #ccc;"
                                    "  padding: 5px;"
                                    "}"
                                    "GtkListBox {"
                                    "  background-color: #f4f4f4;"
                                    "  border: 1px solid #ddd;"
                                    "}",
                                    -1, NULL);

    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    apply_css();
    connect_to_server();

    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0)
    {
        perror("Failed to create receive thread");
        close(sockfd);
        return EXIT_FAILURE;
    }

    login_window = create_login_window();
    chat_window = create_chat_window();
    gtk_widget_show_all(login_window);

    // Connect the application exit event to the on_app_exit function
    g_signal_connect(G_OBJECT(login_window), "destroy", G_CALLBACK(on_app_exit), NULL);
    g_signal_connect(G_OBJECT(chat_window), "destroy", G_CALLBACK(on_app_exit), NULL);

    gtk_main();

    return 0;
}
