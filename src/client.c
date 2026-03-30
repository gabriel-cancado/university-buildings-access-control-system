#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../include/common.h"

void usage_exit(int argc, char** argv) {
    char* progam_name = argv[0];
    printf("usage: %s <servers IP> <users server port> <loc server port> <loc_id>\n", progam_name);
    printf("example: %s 127.0.0.1 50000 6000 2\n", progam_name);

    exit(EXIT_FAILURE);
}

int create_socket(struct sockaddr_storage* storage) {
    int is_ipv4 = storage->ss_family == AF_INET;
}

int open_connection(char* server_addr_str, char* server_port_str) {
    struct sockaddr_storage storage;
    int success = addr_parse(server_addr_str, server_port_str, &storage) == 0;
    if (success == -1) log_exit("Invalid server address. Valid types are IPv4 and IPv6");

    int soc = socket(storage.ss_family, SOCK_STREAM, 0);
    if (soc == -1) log_exit("Error creating socket");

    success = connect(soc, &storage, sizeof(storage));
    if (!success) log_exit("Could not connect to server " + *server_addr_str + ':' + *server_port_str);

    return soc;
}

void main (int argc, char** argv) {
    if (argc < 5) usage_exit(argc, argv);

    int loc_id = atoi(argv[4]);
    if (loc_id < 0 || loc_id > 10) error_exit("Invalid argument");

    int users_server_socket = open_connection(argv[1], argv[2]);
    int loc_server_socket = open_connection(argv[1], argv[3]);

    while(1) {};
}