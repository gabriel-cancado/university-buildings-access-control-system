#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>

void error_exit(char* msg) {
    printf("%s\n", msg);
    exit(EXIT_FAILURE);
}

void log_exit(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int addr_parse(char* addr_str, char* port_str, struct sockaddr_storage *storage) {
    if (addr_str == NULL || port_str == NULL) return -1;

    uint16_t port = (uint16_t)atoi(port_str); // port numbers are 16 bits
    if (port == 0) return -1;

    port = htons(port); // host to network short

    // We first try to parse the address to IPv4, if it doesn`t work we try IPv6
    struct in_addr in_addr_ipv4; // 32-bit IP address
    int is_ipv4_addr = inet_pton(AF_INET, addr_str, &in_addr_ipv4);
    if (is_ipv4_addr) {
        struct sockaddr_in *addr_ipv4 = (struct sockaddr_in *)storage;
        *addr_ipv4 = (struct sockaddr_in){ .sin_family = AF_INET, .sin_addr = in_addr_ipv4, .sin_port = port };
        return 0;
    }

    struct in6_addr in_addr_ipv6; // 128-bit IPv6 address
    int is_ipv6_addr = inet_pton(AF_INET6, addr_str, &in_addr_ipv6);
    if (is_ipv6_addr) {
        struct sockaddr_in6 *addr_ipv6 = (struct sockaddr_in6 *)storage;
        *addr_ipv6 = (struct sockaddr_in6){ .sin6_family = AF_INET6, .sin6_port = port };
        memcpy(&(addr_ipv6->sin6_addr), &in_addr_ipv6, sizeof(in_addr_ipv6));
        return 0;
    }

    return -1;
}