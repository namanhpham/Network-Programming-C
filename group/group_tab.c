#include <gtk/gtk.h>
#include "group_tab.h"

// Declare external variables
extern GtkWidget *chat_group_view;
extern char current_user[256];
GtkWidget *group_message_entry;
GtkWidget *group_list;
GtkWidget *chat_area;
extern char current_group[256]; // Stores the name of the currently selected group
char current_group_creator_name[256];
extern void clear_chat_group_window();
extern int sockfd;


void request_remove_group_member(GtkWidget *widget, gpointer data)
{
    const char *group_name = current_group;
    const char *member_username = gtk_entry_get_text(GTK_ENTRY(data));

    if (strlen(group_name) > 0 && strlen(member_username) > 0)
    {
        char payload[256];
        snprintf(payload, sizeof(payload), "%s:%s", group_name, member_username);

        Message msg = create_message(MSG_REMOVE_GROUP_MEMBER, (uint8_t *)payload, strlen(payload));
        if (send_message(sockfd, &msg) < 0)
        {
            g_print("Failed to request removing group member\n");
        }
        gtk_entry_set_text(GTK_ENTRY(data), "");
    }
    else
    {
        g_print("Group name and member username cannot be empty\n");
    }
}

void update_remove_member_ui(GtkWidget *chat_area, gboolean is_creator) {
    static GtkWidget *remove_member_hbox = NULL;

    if (is_creator && !remove_member_hbox) {
        remove_member_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

        GtkWidget *remove_member_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(remove_member_hbox), remove_member_entry, TRUE, TRUE, 0);

        GtkWidget *remove_button = gtk_button_new_with_label("Remove Member");
        gtk_box_pack_start(GTK_BOX(remove_member_hbox), remove_button, FALSE, FALSE, 0);
        g_signal_connect(remove_button, "clicked", G_CALLBACK(request_remove_group_member), remove_member_entry);

        gtk_box_pack_start(GTK_BOX(chat_area), remove_member_hbox, FALSE, FALSE, 0);
        gtk_widget_show_all(remove_member_hbox);
    } else if (!is_creator && remove_member_hbox) {
        gtk_widget_destroy(remove_member_hbox);
        remove_member_hbox = NULL;
    }
}

void select_group(GtkWidget *widget, gpointer data)
{
    // Kiểm tra dữ liệu truyền vào
    if (!data)
    {
        fprintf(stderr, "Error: No group selected\n");
        return;
    }
    // split data by ":"
    char *group_data = (char *)data;
    char *group_name = strtok(group_data, ":");
    char *creator_name = strtok(NULL, "");

    strncpy(current_group, (const char *)data, sizeof(current_group) - 1);
    current_group[sizeof(current_group) - 1] = '\0';
    printf("Selected group: %s\n", group_name);
    strncpy(current_group_creator_name, creator_name, sizeof(current_group_creator_name) - 1);
    current_group_creator_name[sizeof(current_group_creator_name) - 1] = '\0';

    // Clear chat area and request chat history
    clear_chat_group_window();

    // Gửi yêu cầu lịch sử tin nhắn
    char payload[256];
    snprintf(payload, sizeof(payload), "%s", current_group);
    Message msg = create_message(MSG_GROUP_MSG_HISTORY, (uint8_t *)payload, strlen(payload));
    send_message(sockfd, &msg);

    printf("Selected group: %s\n", current_group);
    printf("Selected group creator: %s\n", current_group_creator_name);
    // Update remove member UI based on creator status
    update_remove_member_ui(chat_area, strcmp(current_user, current_group_creator_name) == 0);
}



// Function to populate the group list dynamically
void update_group_list(const char *groups)
{
    static char *last_groups = NULL;

    // Check if data hasn't changed, skip updating
    if (last_groups && strcmp(last_groups, groups) == 0)
        return;

    // Save the new data
    g_free(last_groups);
    last_groups = g_strdup(groups);

    // Clear old widgets before adding new ones
    gtk_container_foreach(GTK_CONTAINER(group_list), (GtkCallback)gtk_widget_destroy, NULL);

    // Create a copy of the input string for processing
    char *groups_copy = g_strdup(groups);

    // Remove trailing comma if it exists
    size_t len = strlen(groups_copy);
    if (len > 0 && groups_copy[len - 1] == ',')
    {
        groups_copy[len - 1] = '\0';
    }

    // Tokenization context for the outer loop
    char *outer_ctx;
    char *group_details = strtok_r(groups_copy, ",", &outer_ctx);

    while (group_details != NULL)
    {
        // Make a copy of `group_details` to safely tokenize it
        char *details_copy = g_strdup(group_details);

        // Tokenization context for inner parsing
        char *inner_ctx;
        char *group_name = strtok_r(details_copy, ":", &inner_ctx);
        char *creator_name = strtok_r(NULL, ":", &inner_ctx);
        char *member_count = strtok_r(NULL, ":", &inner_ctx);

        if (group_name && creator_name && member_count)
        {
            char display_text[256];
            snprintf(display_text, sizeof(display_text), "Group: %s\nCreator: %s\nMembers: %s",
                     group_name, creator_name, member_count);

            // Create the button and add it to the list
            GtkWidget *group_button = gtk_button_new_with_label(display_text);
            gtk_list_box_insert(GTK_LIST_BOX(group_list), group_button, -1);
            // Tạo chuỗi mới ghép group_name và creator_name
            char formatted_group[128];
            snprintf(formatted_group, sizeof(formatted_group), "%s:%s", group_name, creator_name);
            printf("Formatted group: %s\n", formatted_group);
            // Attach a click event handler for the button
            g_signal_connect(group_button, "clicked", G_CALLBACK(select_group), g_strdup(formatted_group));
        }
        else
        {
            printf("Error parsing group details\n");
        }

        // Free the copy after processing
        g_free(details_copy);

        // Move to the next group
        group_details = strtok_r(NULL, ",", &outer_ctx);
    }

    // Free the copied string
    g_free(groups_copy);

    // Show all widgets in the list
    gtk_widget_show_all(group_list);
}

// Function to send a message to the selected group
void send_message_to_group(GtkWidget *widget, gpointer data)
{
    const char *text = gtk_entry_get_text(GTK_ENTRY(group_message_entry));

    if (strlen(current_group) > 0 && strlen(text) > 0)
    {
        char payload[512];
        snprintf(payload, sizeof(payload), "%s:%s", current_group, text);

        Message msg = create_message(MSG_GROUP_MSG, (uint8_t *)payload, strlen(payload));
        if (send_message(sockfd, &msg) < 0)
        {
            g_print("Failed to send message to group\n");
        }

        gtk_entry_set_text(GTK_ENTRY(group_message_entry), "");
    }
    else
    {
        g_print("Group or message cannot be empty\n");
    }
}

void update_message_history(const char *message)
{
    // Ensure chat_group_view is valid
    if (!GTK_IS_TEXT_VIEW(chat_group_view))
    {
        fprintf(stderr, "Error: chat_group_view is not a valid GtkTextView\n");
        return;
    }

    GtkTextBuffer *buffer_view = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_group_view));
    if (!buffer_view)
    {
        fprintf(stderr, "Error: Failed to get buffer\n");
        return;
    }

    // Always fetch a fresh end iterator before inserting
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(buffer_view, &end_iter);

    // Insert the message at the end of the buffer
    gtk_text_buffer_insert(buffer_view, &end_iter, message, -1);

    // Refresh the iterator for additional modifications (if needed)
    gtk_text_buffer_get_end_iter(buffer_view, &end_iter);
    gtk_text_buffer_insert(buffer_view, &end_iter, "\n", -1);
}


void request_create_group(GtkWidget *widget, gpointer data)
{
    const char *group_name = gtk_entry_get_text(GTK_ENTRY(data));

    if (strlen(group_name) > 0)
    {
        char payload[256];
        snprintf(payload, sizeof(payload), "%s", group_name);

        Message msg = create_message(MSG_CREATE_GROUP, (uint8_t *)payload, strlen(payload));
        if (send_message(sockfd, &msg) < 0)
        {
            g_print("Failed to request creating group\n");
        }
        else
        {
            g_print("Request to create group '%s' sent successfully\n", group_name);
            Message msg2 = create_message(MSG_SEE_JOINED_GROUPS, NULL, 0);
            if (send_message(sockfd, &msg2) < 0)
            {
                g_print("Failed to request list of joined groups\n");
            }
        }

        gtk_entry_set_text(GTK_ENTRY(data), "");
    }
    else
    {
        g_print("Group name cannot be empty\n");
    }
}

// Function to request the list of joined groups
void request_list_joined_groups(GtkWidget *widget, gpointer data)
{
    Message msg = create_message(MSG_SEE_JOINED_GROUPS, NULL, 0);
    if (send_message(sockfd, &msg) < 0)
    {
        g_print("Failed to request list of joined groups\n");
    }
}

// Function to request joining a group
void request_join_group(GtkWidget *widget, gpointer data)
{
    const char *group_name = gtk_entry_get_text(GTK_ENTRY(data));

    if (strlen(group_name) > 0)
    {
        char payload[256];
        snprintf(payload, sizeof(payload), "%s", group_name);

        Message msg = create_message(MSG_JOIN_GROUP, (uint8_t *)payload, strlen(payload));
        if (send_message(sockfd, &msg) < 0)
        {
            g_print("Failed to request joining group\n");
        }

        gtk_entry_set_text(GTK_ENTRY(data), "");
        Message msg2 = create_message(MSG_SEE_JOINED_GROUPS, NULL, 0);
        if (send_message(sockfd, &msg2) < 0)
        {
            g_print("Failed to request list of joined groups\n");
        }
    }
    else
    {
        g_print("Group name cannot be empty\n");
    }
}

// Function to request leaving a group
void request_leave_group(GtkWidget *widget, gpointer data)
{
    const char *group_name = current_group;

    if (strlen(group_name) > 0)
    {
        char payload[256];
        snprintf(payload, sizeof(payload), "%s", group_name);
        Message msg = create_message(MSG_LEAVE_GROUP, (uint8_t *)payload, strlen(payload));
        if (send_message(sockfd, &msg) < 0)
        {
            g_print("Failed to request leaving group\n");
        }
        Message msg2 = create_message(MSG_SEE_JOINED_GROUPS, NULL, 0);
        if (send_message(sockfd, &msg2) < 0)
        {
            g_print("Failed to request list of joined groups\n");
        }
    }
    else
    {
        g_print("Group name cannot be empty\n");
    }
}



// Function to create group tab layout
GtkWidget *create_group_tab()
{
    GtkWidget *group_tab = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    // Left panel: Group list
    GtkWidget *group_list_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget *group_list_label = gtk_label_new("Joined Groups:");
    gtk_box_pack_start(GTK_BOX(group_list_vbox), group_list_label, FALSE, FALSE, 0);

    group_list = gtk_list_box_new();
    gtk_box_pack_start(GTK_BOX(group_list_vbox), group_list, TRUE, TRUE, 0);

    // Join group UI
    GtkWidget *join_group_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(group_list_vbox), join_group_hbox, FALSE, FALSE, 0);

    GtkWidget *group_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(join_group_hbox), group_entry, TRUE, TRUE, 0);

    GtkWidget *join_button = gtk_button_new_with_label("Join Group");
    gtk_box_pack_start(GTK_BOX(join_group_hbox), join_button, FALSE, FALSE, 0);
    g_signal_connect(join_button, "clicked", G_CALLBACK(request_join_group), group_entry);

    // Create group UI
    GtkWidget *create_group_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(group_list_vbox), create_group_hbox, FALSE, FALSE, 0);

    GtkWidget *create_group_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(create_group_hbox), create_group_entry, TRUE, TRUE, 0);

    GtkWidget *create_button = gtk_button_new_with_label("Create Group");
    gtk_box_pack_start(GTK_BOX(create_group_hbox), create_button, FALSE, FALSE, 0);
    g_signal_connect(create_button, "clicked", G_CALLBACK(request_create_group), create_group_entry);

    gtk_paned_pack1(GTK_PANED(group_tab), group_list_vbox, FALSE, FALSE);

    // Right panel: Chat area
    chat_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, -1, 300);

    chat_group_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_group_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), chat_group_view);
    gtk_box_pack_start(GTK_BOX(chat_area), scrolled_window, TRUE, TRUE, 0);

    GtkWidget *input_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(chat_area), input_hbox, FALSE, FALSE, 0);

    group_message_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(input_hbox), group_message_entry, TRUE, TRUE, 0);

    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(input_hbox), send_button, FALSE, FALSE, 0);
    g_signal_connect(send_button, "clicked", G_CALLBACK(send_message_to_group), NULL);

    update_remove_member_ui(chat_area, strcmp(current_user, current_group_creator_name) == 0);

    // Leave group button
    GtkWidget *leave_group_button = gtk_button_new_with_label("Leave Group");
    gtk_box_pack_end(GTK_BOX(chat_area), leave_group_button, FALSE, FALSE, 0);
    g_signal_connect(leave_group_button, "clicked", G_CALLBACK(request_leave_group), NULL);

    gtk_paned_pack2(GTK_PANED(group_tab), chat_area, TRUE, FALSE);

    return group_tab;
}