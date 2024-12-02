#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void load_env_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening .env file");
        return;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // Skip empty lines or comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // Split the line into key and value
        char *delimiter = strchr(line, '=');
        if (!delimiter) {
            fprintf(stderr, "Invalid line in .env file: %s\n", line);
            continue;
        }

        *delimiter = '\0'; // Replace '=' with '\0' to split key and value
        char *key = line;
        char *value = delimiter + 1;

        // Set the environment variable
        if (setenv(key, value, 1) != 0) {
            perror("Error setting environment variable");
        }
    }

    fclose(file);
}