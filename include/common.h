#pragma once

#define MAX_USERS 30;
#define MAX_CLIENTS 10;
#define MAX_PEERS 1;

typedef struct {
    char uid[11];
    int is_special;
} user_auth;

typedef struct {
    char uid[11];
    int last_location;
} user_loc;

void error_exit(char* msg);

void log_exit(char* msg);

/*
* Parses a string to internet address (IPv4 or IPv6)
*/
int addr_parse(char* addr_str, char* port_str, struct sockaddr_storage *storage);