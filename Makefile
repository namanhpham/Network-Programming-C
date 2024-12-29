# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -pthread
LDFLAGS = -lpq

# Source files for server
SERVER_SRCS = common.c \
	   friendship/friendship.c \
       message_handling/message_handling.c \
       network_utils/network_utils.c \
       protocol.c \
       user_management/user_management.c \
	   utils.c \
	   group/group.c \
	   private_message/private_message.c \
	   server.c 

# Source files for client
CLIENT_SRCS = protocol.c client.c

# Source files for client GUI
CLIENT_GUI_SRCS = protocol.c ui/client_gui.c private_message/chat_tab.c friendship/friend_tab.c group/group_tab.c

# Output binaries
SERVER_TARGET = server
CLIENT_TARGET = client
CLIENT_GUI_TARGET = client_gui

# Default target
all: $(SERVER_TARGET) $(CLIENT_TARGET) $(CLIENT_GUI_TARGET)

# Build the server executable
$(SERVER_TARGET): $(SERVER_SRCS)
	$(CC) $(CFLAGS) -o $(SERVER_TARGET) $(SERVER_SRCS) $(LDFLAGS)

# Build the client executable
$(CLIENT_TARGET): $(CLIENT_SRCS)
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_SRCS)

# Build the client GUI executable
$(CLIENT_GUI_TARGET): $(CLIENT_GUI_SRCS)
	$(CC) $(CFLAGS) `pkg-config --cflags gtk+-3.0` -o $(CLIENT_GUI_TARGET) $(CLIENT_GUI_SRCS) `pkg-config --libs gtk+-3.0`

# Clean up build artifacts
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET) $(CLIENT_GUI_TARGET)

# Run the server
run_server: $(SERVER_TARGET)
	./$(SERVER_TARGET)

# Run the client
run_client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET)

# Run the client GUI
run_client_gui: $(CLIENT_GUI_TARGET)
	./$(CLIENT_GUI_TARGET)
