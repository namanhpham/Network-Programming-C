# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -pthread

# Source files
SRCS = common.c \
	   friendship/friendship.c \
       message_handling/message_handling.c \
       network_utils/network_utils.c \
       protocol.c \
       user_management/user_management.c \
	   server.c 

# Output binary
TARGET = server

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Clean up build artifacts
clean:
	rm -f $(TARGET)

# Run the compiled program
run: $(TARGET)
	./$(TARGET)
