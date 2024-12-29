#include <gtk/gtk.h>
#include "../protocol.h"

GtkWidget *friends_list_box;
GtkWidget *friend_requests_box;
GtkWidget *search_entry;
GtkWidget *friend_tab_stack;
GtkWidget *friend_sidebar;

extern int sockfd;

// Function to send a friend request
void on_send_friend_request_clicked(GtkWidget *widget, gpointer data)
{
    const char *friend_name = gtk_entry_get_text(GTK_ENTRY(search_entry));
    if (strlen(friend_name) > 0)
    {
        Message msg = create_message(MSG_FRIEND_REQUEST, (uint8_t *)friend_name, strlen(friend_name));
        if (send_message(sockfd, &msg) < 0)
        {
            g_print("Failed to send friend request\n");
        }
        else
        {
            gtk_entry_set_text(GTK_ENTRY(search_entry), "");
            g_print("Friend request sent to %s\n", friend_name);
        }
    }
}

// Function to remove a friend
void on_remove_friend_clicked(GtkWidget *button, gpointer friend_name)
{
    Message msg = create_message(MSG_FRIEND_REMOVED, (uint8_t *)friend_name, strlen(friend_name));
    if (send_message(sockfd, &msg) < 0)
    {
        g_print("Failed to remove friend\n");
    }
}

// Function to accept a friend request
void on_accept_friend_request_clicked(GtkWidget *button, gpointer friend_name)
{
    Message msg = create_message(MSG_FRIEND_REQUEST_ACCEPTED, (uint8_t *)friend_name, strlen(friend_name));
    if (send_message(sockfd, &msg) < 0)
    {
        g_print("Failed to accept friend request\n");
    }
}

// Function to decline a friend request
void on_decline_friend_request_clicked(GtkWidget *button, gpointer friend_name)
{
    Message msg = create_message(MSG_FRIEND_REQUEST_DECLINED, (uint8_t *)friend_name, strlen(friend_name));
    if (send_message(sockfd, &msg) < 0)
    {
        g_print("Failed to decline friend request\n");
    }
}

// Function to populate friends list
void populate_friends_list(const char *friends)
{
    gtk_container_foreach(GTK_CONTAINER(friends_list_box), (GtkCallback)gtk_widget_destroy, NULL);

    char *friends_copy = strdup(friends);
    char *friend = strtok(friends_copy, "\n");
    while (friend)
    {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        GtkWidget *label = gtk_label_new(friend);
        GtkWidget *remove_button = gtk_button_new_with_label("Remove");

        g_signal_connect(remove_button, "clicked", G_CALLBACK(on_remove_friend_clicked), g_strdup(friend));

        gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), remove_button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(friends_list_box), row, FALSE, FALSE, 0);

        friend = strtok(NULL, "\n");
    }
    free(friends_copy);
    gtk_widget_show_all(friends_list_box);
}

// Function to populate friend requests
void populate_friend_requests(const char *requests)
{
    gtk_container_foreach(GTK_CONTAINER(friend_requests_box), (GtkCallback)gtk_widget_destroy, NULL);

    char *requests_copy = strdup(requests);
    char *request = strtok(requests_copy, "\n");
    while (request)
    {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        GtkWidget *label = gtk_label_new(request);
        GtkWidget *accept_button = gtk_button_new_with_label("Accept");
        GtkWidget *decline_button = gtk_button_new_with_label("Decline");

        g_signal_connect(accept_button, "clicked", G_CALLBACK(on_accept_friend_request_clicked), g_strdup(request));
        g_signal_connect(decline_button, "clicked", G_CALLBACK(on_decline_friend_request_clicked), g_strdup(request));

        gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), accept_button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), decline_button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(friend_requests_box), row, FALSE, FALSE, 0);

        request = strtok(NULL, "\n");
    }
    free(requests_copy);
    gtk_widget_show_all(friend_requests_box);
}

void on_switch_page_clicked(GtkWidget *button, gpointer data)
{
    GtkStack *stack = GTK_STACK(data);
    const char *page_name = g_object_get_data(G_OBJECT(button), "page-name");

    if (!GTK_IS_STACK(stack))
    {
        g_print("Error: The provided widget is not a stack.\n");
        return;
    }

    if (!page_name)
    {
        g_print("Error: Page name not found in button data.\n");
        return;
    }

    gtk_stack_set_visible_child_name(stack, page_name);
}

GtkWidget *create_friend_tab()
{
    GtkWidget *friend_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    // Create the sidebar
    friend_sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(friend_sidebar, 150, -1);

    GtkWidget *switch_to_friends_button = gtk_button_new_with_label("Friends List");
    GtkWidget *switch_to_requests_button = gtk_button_new_with_label("Friend Requests");

    // Create the stack before setting signals
    friend_tab_stack = gtk_stack_new();

    // Set data for the buttons
    g_object_set_data(G_OBJECT(switch_to_friends_button), "page-name", "friends");
    g_object_set_data(G_OBJECT(switch_to_requests_button), "page-name", "requests");

    gtk_box_pack_start(GTK_BOX(friend_sidebar), switch_to_friends_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(friend_sidebar), switch_to_requests_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(friend_hbox), friend_sidebar, FALSE, FALSE, 0);

    // Create pages and add to stack
    GtkWidget *friends_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    // Add search bar and button in a horizontal box
    GtkWidget *search_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search for a friend...");
    GtkWidget *send_request_button = gtk_button_new_with_label("Send Request");
    g_signal_connect(send_request_button, "clicked", G_CALLBACK(on_send_friend_request_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(search_hbox), search_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(search_hbox), send_request_button, FALSE, FALSE, 0);

    friends_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(friends_page), search_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(friends_page), friends_list_box, TRUE, TRUE, 0);

    gtk_stack_add_titled(GTK_STACK(friend_tab_stack), friends_page, "friends", "Friends List");

    GtkWidget *requests_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    friend_requests_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(requests_page), friend_requests_box, TRUE, TRUE, 0);

    gtk_stack_add_titled(GTK_STACK(friend_tab_stack), requests_page, "requests", "Friend Requests");

    // Set stack visibility and pack it into the container
    gtk_stack_set_visible_child_name(GTK_STACK(friend_tab_stack), "friends");
    gtk_box_pack_start(GTK_BOX(friend_hbox), friend_tab_stack, TRUE, TRUE, 0);

    // Connect signals after stack initialization
    g_signal_connect(switch_to_friends_button, "clicked", G_CALLBACK(on_switch_page_clicked), friend_tab_stack);
    g_signal_connect(switch_to_requests_button, "clicked", G_CALLBACK(on_switch_page_clicked), friend_tab_stack);

    return friend_hbox;
}
