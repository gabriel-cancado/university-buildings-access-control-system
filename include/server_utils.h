#pragma once
#include "common.h"

typedef struct {
    int user_id;
    int is_special;
} user_auth;

typedef struct {
    user_auth users_auth[MAX_USERS];
    int active_users;
} users_server_database;

typedef struct {
    int user_id;
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

p2p_connection_info handle_pairing_requests(int listener_socket);

p2p_connection_info request_connection_to_peer(int soc, struct sockaddr_in6 addr6);

void kill_p2p_connection(p2p_connection_info peer_connection_info, int p2p_connections_listener_socket);

void handle_REQDISCPEER (p2p_connection_info* peer_connection_info, message request);

void handle_REQDISC(clients_connection_info* clients_info, client c, message request);

void handle_REQUSRADD(users_server_database* db, client c, message_payload request_payload);

user_auth* find_user_auth(users_server_database* db, int user_id);