#!/bin/bash
# Check if a program name is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <program_name>"
    exit 1
fi

# Navigate to the directory of the script
cd "$(dirname "$0")"

# Compile the project
gcc -o "$1" \
    "$1".c \
    friendship/friendship.c \
    message_handling/message_handling.c \
    network_utils/network_utils.c \
    protocol.c \
    user_management/user_management.c \
    -lpthread  # Linking the pthread library as it seems your server might be using threads

# Run the compiled program
./"$1"
