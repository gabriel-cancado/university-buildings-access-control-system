#include "../include/common.h"
#include "../include/server_utils.h"

#define MAXIMUM_AMOUNT_OF_PEERS 1

int peers_connected_count = 0;

p2p_connection_info handle_pairing_requests(int listener_socket) {
    int new_peer_socket = accept(listener_socket, NULL, NULL);
    if (new_peer_socket == -1) log_exit("Error accepting connection from peer");

    message request;
    int received_bytes = recv(new_peer_socket, &request, sizeof(request), 0);
    if (received_bytes == -1) log_exit("Error receiving message");

    if (request.code != REQ_CONNPEER) log_exit("Error: Peer tried to communicate before REQ_CONNPEER");

    if (peers_connected_count >= MAXIMUM_AMOUNT_OF_PEERS) {
        message msg = {
            .code = ERROR,
            .payload = {
                .description = "Peer limit exceeded"
            }
        };
        
        send_message(new_peer_socket, msg, false);
        close(new_peer_socket);

        p2p_connection_info no_peer = { .connected = false };
        return no_peer;
    }

    int peer_id = generate_random_id();
    printf("Peer %d connected\n", peer_id);
    
    message msg = {
        .code = RES_CONNPEER,
        .payload = {
            .peer_id = peer_id
        }
    };
    message response = send_message(new_peer_socket, msg, true);

    int id_on_peer = response.payload.peer_id;
    printf("New Peer ID: %d\n", response.payload.peer_id);
    peers_connected_count++;
    
    p2p_connection_info connection_info = {
        .connected = true,
        .id_on_peer = id_on_peer,
        .peer_id = peer_id,
        .soc = new_peer_socket
    };
    return connection_info;
}

void kill_p2p_connection(p2p_connection_info peer_connection_info, int p2p_connections_listener_socket) {
    /**
     * Closing connections listener socket before REQ_DISCPEER so that the other server
     * can start to listen for connections in the same address.
     */
    close(p2p_connections_listener_socket);

    message msg = {
        .code = REQ_DISCPEER,
        .payload = {
            .peer_id = peer_connection_info.id_on_peer
        }
    };

    message response = send_message(peer_connection_info.soc, msg, true);
    if (response.code == ERROR) {
        printf("%s\n", response.payload.description);
        exit(EXIT_FAILURE);
    }

    if (response.code != OK) log_exit("Unexpected response code");

    printf("%s\n", response.payload.description);
    printf("Peer %d disconnected\n", peer_connection_info.peer_id);

    exit(EXIT_SUCCESS);
}

p2p_connection_info request_connection_to_peer(int soc, struct sockaddr_in6 addr6) {
    int success = connect(soc, (struct sockaddr*) &addr6, sizeof(addr6));
    if (success == -1) {
        p2p_connection_info no_peer = { .connected = false };
        return no_peer;
    }
    
    message request = { .code = REQ_CONNPEER };

    message response = send_message(soc, request, true);
    if (response.code == ERROR) {
        printf("%s\n", response.payload.description);
        exit(EXIT_FAILURE);
    }

    int server_id_on_peer = response.payload.peer_id;
    printf("New Peer ID: %d\n", server_id_on_peer);

    int peer_id = generate_random_id();
    message msg = {
        .code = RES_CONNPEER,
        .payload = {
            .peer_id = peer_id
        }
    };
    send_message(soc, msg, false);

    printf("Peer %d connected\n", peer_id);
    peers_connected_count++;

    p2p_connection_info connection_info = { 
        .connected = true,
        .id_on_peer = server_id_on_peer,
        .peer_id = peer_id,
        soc: soc
    };
    return connection_info;
}

void handle_REQDISCPEER (p2p_connection_info* peer_connection_info, message request) {
    int disconnect_requested_id = request.payload.peer_id;
    if (disconnect_requested_id != peer_connection_info->peer_id) {
        message msg = {
            .code = ERROR,
            .payload = {
                .description = "Peer not found"
            }
        };
        send_message(peer_connection_info->soc, msg, false);
        return;
    }

    message msg = {
        .code = OK,
        .payload = {
            .description = "Successful disconnect"
        }
    };
    send_message(peer_connection_info->soc, msg, false);

    printf("Peer %d disconnected\n", peer_connection_info->peer_id);

    peer_connection_info->connected = false;
    close(peer_connection_info->soc);
    peers_connected_count--;
    return;
}

void remove_client(clients_connection_info* clients_info, int client_id) {
    int client_index = -1;
    for (int i = 0; i < clients_info->connected_clients; i++) {
        if (clients_info->clients[i].id != client_id) continue;
        client_index = i;
    }

    if (client_index == -1) log_exit("Client not found");

    close(clients_info->clients[client_index].soc);
    for (int i = client_index; i < clients_info->connected_clients - 1; i++) {
        clients_info->clients[i] = clients_info->clients[i + 1];
    }
    clients_info->connected_clients--;
}

void handle_REQDISC(clients_connection_info* clients_info, client c, message request) {
    bool isSameId = request.payload.client_id == c.id;
    if (!isSameId) {
        message response = {
            .code = ERROR,
            .payload = { .description_code = 10 }
        };

        send_message(c.soc, response, false);
        return;
    }

    message response = {
        .code = OK,
        .payload = { .description_code = 1 }
    };

    send_message(c.soc, response, false);

    remove_client(clients_info, c.id);
    printf("Client %d removed (Loc %d)\n", c.id, c.loc);
}

user_auth* find_user_auth(users_server_database* db, int user_id) {
    for (int i = 0; i < db->active_users; i++) {
        if (db->users_auth[i].user_id == user_id) {
            return &db->users_auth[i];
        }
    }

    return NULL;
}

void add_user_auth(users_server_database* db, user_auth* new_user) {
    db->users_auth[db->active_users] = *new_user;
    db->active_users++;
}

void handle_REQUSRADD(users_server_database* db, client c, message_payload request_payload) {
    int user_id = request_payload.user_id;
    int is_special = request_payload.is_special;

    printf("REQ_USRADD %d %d\n", user_id, is_special);

    user_auth* user = find_user_auth(db, user_id);
    if (user != NULL) {
        user->is_special = is_special;
        message response = { .code = OK, .payload = { .description_code = 3 } };
        send_message(c.soc, response, false);
        return;
    }

    if (db->active_users >= MAX_USERS) {
        message response = { .code = ERROR, .payload = { .description_code = 17 } };
        send_message(c.soc, response, false);
        return;
    }

    user_auth new_user = { .user_id = user_id, .is_special = is_special };
    add_user_auth(db, &new_user);

    message response = { .code = OK, .payload = { .description_code = 2 } };
    send_message(c.soc, response, false);
}