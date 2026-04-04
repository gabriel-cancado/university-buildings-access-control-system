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
#include "../include/common.h"

#define CONNECTION_QUEUE_MAX_LENGTH 10
#define MAXIMUM_AMOUNT_OF_PEERS 1

int peers_connected_count = 0;

typedef struct {
    bool connected;
    int peer_id;
    int id_on_peer;
    int soc;
} p2p_connection_info;

void usage_exit(int argc, char** argv) {
    char* progam_name = argv[0];
    printf("usage: %s <peer-2-peer port> <clients port>\n", progam_name);
    printf("example: %s 40000 50000\n", progam_name);

    exit(EXIT_FAILURE);
}

int generate_random_id() {
    return (rand() % 9000) + 1000;
}

void server_sockaddr_init(struct sockaddr_in6* addr6, char* port_str) {
    uint16_t port = (uint16_t) atoi(port_str);
    port = htons(port); // host to network short
    if (port == 0) log_exit("Invalid port number");

    addr6->sin6_family = AF_INET6;
    addr6->sin6_addr = in6addr_any;
    addr6->sin6_port = port;
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

int listen_for_p2p_connections(int soc, struct sockaddr_in6 addr6) {
    int success = bind(soc, (struct sockaddr*) &addr6, sizeof(addr6));
    if (success == -1) log_exit("Could not bind p2p socket to IP address");

    success = listen(soc, CONNECTION_QUEUE_MAX_LENGTH);
    if (success == -1) log_exit("Could not listen on p2p socket");

    printf("No peer found, starting to listen...\n");
}

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

void handle_keyboard_entry(p2p_connection_info peer_connection_info, int p2p_connections_listener_socket) {
    char buffer[1024];
    fgets(buffer, sizeof(buffer), stdin);

    if (strcmp(buffer, "kill\n") == 0) {
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

        close(p2p_connections_listener_socket);
        exit(EXIT_SUCCESS);
    }
}

void handle_request_from_peer(p2p_connection_info* peer_connection_info) {
    message request;
    int received_bytes = recv(peer_connection_info->soc, &request, sizeof(request), 0);
    if (received_bytes == -1) log_exit("Error receiving message");

    if (request.code == REQ_DISCPEER) {
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
}

void monitored_sockets_init(fd_set* monitored_sockets, p2p_connection_info peer_connection_info, int p2p_connections_listener_socket) {
    FD_ZERO(monitored_sockets);

    bool isConnectedToPeer = peer_connection_info.connected;
    if (isConnectedToPeer) {
        FD_SET(peer_connection_info.soc, monitored_sockets);
    }

    bool serverIsListeningForConnectionRequests = p2p_connections_listener_socket != -1;
    if (serverIsListeningForConnectionRequests) {
        FD_SET(p2p_connections_listener_socket, monitored_sockets);
    }

    FD_SET(STDIN_FILENO, monitored_sockets);
}

void handle_requests_loop(p2p_connection_info peer_connection_info, int p2p_connections_listener_socket, struct sockaddr_in6 p2p_server_addr) {
    /**
     * The select function is a synchronous call that blocks the current running process.
     * It works by monitoring a set of sockets and putting then to sleep. When one of those sockets receives
     * a message, it puts that message in a queue and wake up this socket, unblocking the process.
     */
    while(1) {
        fd_set monitored_sockets;
        monitored_sockets_init(&monitored_sockets, peer_connection_info, p2p_connections_listener_socket);
        
        int activity = select(FD_SETSIZE, &monitored_sockets, NULL, NULL, NULL);

        bool isRequestToPair = p2p_connections_listener_socket != -1 && FD_ISSET(p2p_connections_listener_socket, &monitored_sockets);
        if (isRequestToPair) {
            p2p_connection_info pair_result = handle_pairing_requests(p2p_connections_listener_socket);
            if (pair_result.connected == false) continue;

            peer_connection_info = pair_result;
        }

        bool isKeyboardEntry = FD_ISSET(STDIN_FILENO, &monitored_sockets);
        if (isKeyboardEntry) {
            handle_keyboard_entry(peer_connection_info, p2p_connections_listener_socket);
        }

        bool isRequestFromPeer = FD_ISSET(peer_connection_info.soc, &monitored_sockets);
        if (isRequestFromPeer) {
            handle_request_from_peer(&peer_connection_info);

            bool peer_disconnected = !peer_connection_info.connected;
            if (peer_disconnected && p2p_connections_listener_socket == -1) {
                p2p_connections_listener_socket = create_socket();
                // TODO: BUG - Could not bind p2p socket to IP address: Address already in use
                listen_for_p2p_connections(p2p_connections_listener_socket, p2p_server_addr);
            }
        }
    }
}

void main(int argc, char** argv) {
    if (argc < 3) usage_exit(argc, argv);

    srand(time(NULL) ^ getpid());

    /**
     * sockaddr_in6 is a struct for IPv6 addresses. It has a family, that represents IPv6,
     * a addr, that is the server address, and a port, the port number for the server. 
     */
    struct sockaddr_in6 p2p_server_addr;
    server_sockaddr_init(&p2p_server_addr, argv[1]);

    /**
     * Separate the logic of communicating and listening for connection requests
     * in 2 different sockets.
     */
    int peer_communication_socket = -1;
    int p2p_connections_listener_socket = -1;

    /**
     * First, we try to connect to an already open server;
     * if there isn't one, we start listening for incoming pairing requests.
     */
    int p2p_openning_connection_socket = create_socket();
    p2p_connection_info peer_connection_info  = request_connection_to_peer(p2p_openning_connection_socket, p2p_server_addr);

    if (!peer_connection_info.connected) {
        p2p_connections_listener_socket = p2p_openning_connection_socket;
        listen_for_p2p_connections(p2p_connections_listener_socket, p2p_server_addr);

        peer_connection_info = handle_pairing_requests(p2p_connections_listener_socket);
    }

    handle_requests_loop(peer_connection_info, p2p_connections_listener_socket, p2p_server_addr);
}