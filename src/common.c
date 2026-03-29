#include <stdlib.h>
#include <stdio.h>

void usage_exit(int argc, char** argv) {
    char* progam_name = argv[0];
    printf("usage: %s <server IP> <server port>\n", progam_name);
    printf("example: %s 127.0.0.1 5000\n", progam_name);

    exit(EXIT_FAILURE);
}

void log_exit(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}