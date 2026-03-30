#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "../include/common.h"

void usage_exit(int argc, char** argv) {
    char* progam_name = argv[0];
    printf("usage: %s <peer-2-peer port> <clients port>\n", progam_name);
    printf("example: %s 40000 50000\n", progam_name);

    exit(EXIT_FAILURE);
}

int server_sockaddr_init(struct sockaddr_in6* addr6, char* port_str) {
    uint16_t port = (uint16_t) atoi(port_str);
    port = htons(port); // host to network short
    if (port == 0) return -1;

    addr6->sin6_family = AF_INET6;
    addr6->sin6_addr = in6addr_any;
    addr6->sin6_port = port;
    return 0;
}

int create_socket() {
    int soc = socket(AF_INET6, SOCK_STREAM, 0);
    if (soc == -1) log_exit("Error creating socket");

    // Set socket option to reuse port number when process is killed
    int enable = 1;
    int success = setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (success == -1) log_exit("Could not set socket option to reuse port number");

    // Set socket option to accept both IPv4 and IPv6 connections
    int disable_only_ipv6_mode = 0;
    success = setsockopt(soc, IPPROTO_IPV6, IPV6_V6ONLY, &disable_only_ipv6_mode, sizeof(disable_only_ipv6_mode));
    if (success == -1) log_exit("Could not set socket option to accept both IPv4 and IPv6");

    return soc;
}

void open_p2p_connection(int* socket, char* port_str) {
    struct sockaddr_in6 addr6;
    int success = server_sockaddr_init(&addr6, port_str);
    if (success == -1) log_exit("Invalid port number");

    // If the connection was estabilished, that means that the other server was already running
    success = connect(*socket, (struct sockaddr*) &addr6, sizeof(addr6));
    if (success == -1) {
        printf("No peer found, starting to listen...\n");
        return;
    }
}

void main(int argc, char** argv) {
    if (argc < 3) usage_exit(argc, argv);

    int p2p_socket = create_socket();
    open_p2p_connection(&p2p_socket, argv[1]);
}