#pragma once
#include "common.h"

p2p_connection_info handle_pairing_requests(int listener_socket);

p2p_connection_info request_connection_to_peer(int soc, struct sockaddr_in6 addr6);

void kill_p2p_connection(p2p_connection_info peer_connection_info, int p2p_connections_listener_socket);

void handle_REQDISCPEER (p2p_connection_info* peer_connection_info, message request);
