#include "../include/common.h"
#include "../include/request_handlers.h"

#define CONNECTION_QUEUE_MAX_LENGTH 10

void usage_exit(int argc, char** argv) {
    char* progam_name = argv[0];
    printf("usage: %s <peer-2-peer port> <clients port>\n", progam_name);
    printf("example: %s 40000 50000\n", progam_name);

    exit(EXIT_FAILURE);
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

int listen_for_connections(int soc, struct sockaddr_in6 addr6) {
    int success = bind(soc, (struct sockaddr*) &addr6, sizeof(addr6));
    if (success == -1) log_exit("Could not bind p2p socket to IP address");

    success = listen(soc, CONNECTION_QUEUE_MAX_LENGTH);
    if (success == -1) log_exit("Could not listen on p2p socket");
}

void handle_keyboard_entry(p2p_connection_info peer_connection_info, int p2p_connections_listener_socket) {
    char command[1024];
    fgets(command, sizeof(command), stdin);

    if (strcmp(command, "kill\n") == 0) {
        kill_p2p_connection(peer_connection_info, p2p_connections_listener_socket);
    }
}

void handle_request_from_peer(p2p_connection_info* peer_connection_info) {
    message request;
    int received_bytes = recv(peer_connection_info->soc, &request, sizeof(request), 0);
    if (received_bytes == -1) log_exit("Error receiving message");

    if (request.code == REQ_DISCPEER) {
        handle_REQDISCPEER(peer_connection_info, request);
        return;
    }
}

void monitored_sockets_init(
    fd_set* monitored_sockets,
    p2p_connection_info peer_connection_info,
    int p2p_connections_listener_socket,
    clients_connection_info clients_info
) {
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
    FD_SET(clients_info.connections_listener_socket, monitored_sockets);

    for (int i = 0; i < clients_info.connected_clients; i++) {
        FD_SET(clients_info.clients[i].soc, monitored_sockets);
    }
}

int add_client(clients_connection_info* clients_info, int loc, int soc) {
    int new_client_id = clients_info->client_id_sequence++;
    client new_client = {
        .id = new_client_id,
        .loc = loc,
        .soc = soc
    };

    clients_info->clients[clients_info->connected_clients - 1] = new_client;
    clients_info->connected_clients++;

    printf("Client %d added (Loc %d)\n", new_client_id, loc);
    return new_client_id;
}

void handle_client_connection_request(clients_connection_info* clients_info) {
    int new_client_socket = accept(clients_info->connections_listener_socket, NULL, NULL);
    
    if (clients_info->connected_clients == MAX_CLIENTS) {
        message msg = {
            .code = ERROR,
            .payload = {
                .description = "Client limit exceeded"
            }
        };
        send_message(new_client_socket, msg, false);
        close(new_client_socket);
        return;
    }

    message request;
    int received_bytes = recv(new_client_socket, &request, sizeof(request), 0);
    if (received_bytes == -1) log_exit("Error receiving message");

    if (request.code != REQ_CONN) log_exit("Error: Client tried to communicate before REQ_CONN");

    int locId = request.payload.loc_id;
    if (!locId) log_exit("Error: Client tried to connect without a loc_id");

    int client_id = add_client(clients_info, locId, new_client_socket);

    message response = {
        .code = RES_CONN,
        .payload = {
            .client_id = client_id
        }
    };
    send_message(new_client_socket, response, false);
}

void handle_requests_loop(
    p2p_connection_info peer_connection_info,
    int p2p_connections_listener_socket,
    struct sockaddr_in6 p2p_server_addr,
    clients_connection_info clients_info
) {
    /**
     * The select function is a synchronous call that blocks the current running process.
     * It works by monitoring a set of sockets and putting then to sleep. When one of those sockets receives
     * a message, it puts that message in a queue and wake up this socket, unblocking the process.
     */
    while(1) {
        fd_set monitored_sockets;
        monitored_sockets_init(&monitored_sockets, peer_connection_info, p2p_connections_listener_socket, clients_info);
        
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
            if (peer_disconnected) {
                printf("No peer found, starting to listen...\n");

                if (p2p_connections_listener_socket == -1) {
                    p2p_connections_listener_socket = create_socket();
                    listen_for_connections(p2p_connections_listener_socket, p2p_server_addr);
                }
            }
        }

        bool isClientRequestingConnection = FD_ISSET(clients_info.connections_listener_socket, &monitored_sockets);
        if (isClientRequestingConnection) {
            handle_client_connection_request(&clients_info);
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

        printf("No peer found, starting to listen...\n");
        listen_for_connections(p2p_connections_listener_socket, p2p_server_addr);

        peer_connection_info = handle_pairing_requests(p2p_connections_listener_socket);
    }

    struct sockaddr_in6 client_server_addr6;
    server_sockaddr_init(&client_server_addr6, argv[2]);

    int client_connection_listener_socket = create_socket();
    listen_for_connections(client_connection_listener_socket, client_server_addr6);
    clients_connection_info clients_info = {
        .connections_listener_socket = client_connection_listener_socket,
        .connected_clients = 0,
        .client_id_sequence = generate_random_id()
    };

    handle_requests_loop(peer_connection_info, p2p_connections_listener_socket, p2p_server_addr, clients_info);
}