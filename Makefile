# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -pthread
LDFLAGS = -lpq

# Source files
SRCS = common.c \
	   friendship/friendship.c \
       message_handling/message_handling.c \
       network_utils/network_utils.c \
       protocol.c \
       user_management/user_management.c \
	   private_message/private_message.c \
	   utils.c \
	   group/group.c \
	   server.c 

ifneq (,$(findstring server.c,$(SRCS)))
  CFLAGS += -lpq
endif

# Output binary
TARGET = server

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# Clean up build artifacts
clean:
	rm -f $(TARGET)

# Run the compiled program
run: $(TARGET)
	./$(TARGET)

# Client application
CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0`
SRC = client_gui.c protocol.c
OBJ = $(SRC:.c=.o)
EXEC = client

client: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean_client:
	rm -f $(OBJ) $(EXEC)

run_client: $(EXEC)
	./$(EXEC)

.PHONY: all clean run client clean_client run_client
