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

int request_connection_to_peer(int *soc, struct sockaddr_in6 *addr6) {
    int success = connect(*soc, (struct sockaddr*) addr6, sizeof(*addr6));
    if (success == -1) return -1;
    
    message request = { .code = REQ_CONNPEER };
    message response = send_message(soc, request, true);

    if (response.code == ERROR) {
        printf("%s", response.payload.description);
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
    return peer_id;
}

int listen_for_p2p_connections(int* soc, struct sockaddr_in6 *addr6) {
    int success = bind(*soc, (struct sockaddr*) addr6, sizeof(*addr6));
    if (success == -1) log_exit("Could not bind p2p socket to IP address");

    success = listen(*soc, CONNECTION_QUEUE_MAX_LENGTH);
    if (success == -1) log_exit("Could not listen on p2p socket");

    printf("No peer found, starting to listen...\n");
}

int handle_pairing_requests(int listener_socket, int* peer_communication_socket) {
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
                .description = "Peer limit exceeded\n"
            }
        };
        
        send_message(&new_peer_socket, msg, false);
        close(new_peer_socket);
        return -1;
    }

    int peer_id = generate_random_id();
    printf("Peer %d connected\n", peer_id);
    
    message msg = {
        .code = RES_CONNPEER,
        .payload = {
            .peer_id = peer_id
        }
    };
    message response = send_message(&new_peer_socket, msg, true);

    printf("New Peer ID: %d\n", response.payload.peer_id);
    
    *peer_communication_socket = new_peer_socket;
    peers_connected_count++;
    return peer_id;
}

void handle_requests_loop(int peer_communication_socket, int p2p_connections_listener_socket) {
    /**
     * The select function is a synchronous call that blocks the current running process.
     * It works by monitoring a set of sockets and putting then to sleep. When one of those sockets receives
     * a message, it puts that message in a queue and wake up this socket, unblocking the process.
     */
    fd_set monitored_sockets;
    FD_ZERO(&monitored_sockets);
    
    FD_SET(peer_communication_socket, &monitored_sockets);
    if (p2p_connections_listener_socket != -1) {
        FD_SET(p2p_connections_listener_socket, &monitored_sockets);
    }

    // TODO: Remove keyboard inputs in server processes.
    int keyboard_fd = STDIN_FILENO;
    FD_SET(keyboard_fd, &monitored_sockets);

    while(1) {
        fd_set sleeping_sockets = monitored_sockets;
        
        int activity = select(FD_SETSIZE, &sleeping_sockets, NULL, NULL, NULL);

        if (FD_ISSET(keyboard_fd, &sleeping_sockets)) {
            char buffer[1024];
            fgets(buffer, sizeof(buffer), stdin);

            printf("Sending message: %s\n", buffer);
            
            // Envia para o outro servidor
            int bytes_enviados = send(peer_communication_socket, buffer, strlen(buffer), 0);
            if (bytes_enviados == -1) {
                perror("Erro critico no send"); // perror imprime o motivo exato do erro do SO
            } else {
                printf(">> O Sistema Operacional confirmou o envio de %d bytes.\n", bytes_enviados);
            }
        }

        if (FD_ISSET(peer_communication_socket, &sleeping_sockets)) {
            char buffer[1024];
            int bytes_recebidos = recv(peer_communication_socket, buffer, sizeof(buffer), 0);
            
            if (bytes_recebidos == 0) {
                // O outro servidor fechou a conexão (Ctrl+C ou encerrou)
                printf("Conexão encerrada pelo peer.\n");
                break; 
            } else if (bytes_recebidos > 0) {
                // Imprime a mensagem recebida na tela
                buffer[bytes_recebidos] = '\0'; // Garante o fim da string
                printf("Mensagem recebida: %s\n", buffer);
            }
        }

        if (p2p_connections_listener_socket != -1 && FD_ISSET(p2p_connections_listener_socket, &sleeping_sockets)) {
            handle_pairing_requests(p2p_connections_listener_socket, NULL);
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
    int peer_id = request_connection_to_peer(&p2p_openning_connection_socket, &p2p_server_addr);

    if (peer_id != -1) {
        peer_communication_socket = p2p_openning_connection_socket;
    }
    else {
        p2p_connections_listener_socket = p2p_openning_connection_socket;
        listen_for_p2p_connections(&p2p_connections_listener_socket, &p2p_server_addr);

        peer_id = handle_pairing_requests(p2p_connections_listener_socket, &peer_communication_socket);
    }

    handle_requests_loop(peer_communication_socket, p2p_connections_listener_socket);
}