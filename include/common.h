#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define MAX_USERS 30
#define MAX_CLIENTS 10
#define MAX_PEERS 1

#define DIRECTION_IN 0
#define DIRECTION_OUT 1

// Control messages code
#define REQ_CONNPEER 17
#define RES_CONNPEER 18
#define REQ_DISCPEER 19
#define REQ_CONN 20
#define RES_CONN 21
#define REQ_DISC 22

// Data messages code
#define REQ_USRADD 33
#define REQ_USRACCESS 34
#define RES_USRACCESS 35
#define REQ_LOCREG 36
#define RES_LOCREG 37
#define REQ_USRLOC 38
#define RES_USRLOC 39
#define REQ_LOCLIST 40
#define RES_LOCLIST 41
#define REQ_USRAUTH 42
#define RES_USRAUTH 43

// Error and confirm messages code
#define ERROR 255
#define OK 0
#define EMPTY -1

#define DESCRIPTION_MAX_SIZE 128

typedef struct {
    int peer_id;
    int client_id;
    int loc_id;
    int user_id;
    bool is_special;
    int direction;
    char description[DESCRIPTION_MAX_SIZE];
    int description_code;
} message_payload;

typedef struct {
    int code;
    message_payload payload;
} message;

typedef struct {
    char uid[11];
    int is_special;
} user_auth;

typedef struct {
    char uid[11];
    int last_location;
} user_loc;

typedef struct {
    bool connected;
    int peer_id;
    int id_on_peer;
    int soc;
} p2p_connection_info;

typedef struct {
    int id;
    int soc;
    int loc;
} client;

typedef struct {
    int connections_listener_socket;
    int connected_clients;
    client clients[MAX_CLIENTS];
    int client_id_sequence;
} clients_connection_info;

void error_exit(char* msg);

void log_exit(char* msg);

/*
* Parses a string to internet address (IPv4 or IPv6)
*/
int addr_parse(char* addr_str, char* port_str, struct sockaddr_storage *storage);

message send_message(int soc, message msg, bool should_wait_response);

int generate_random_id();

char* get_message_description(int type, int code);