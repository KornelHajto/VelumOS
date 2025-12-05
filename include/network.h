#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"
#include <netinet/in.h>

#define BASE_PORT 8000
#define MAX_PEERS 10
#define NUM_NODES 5

int setup_server(int port);

int connect_to_peer(int peer_id, char *ip);

void send_message(int socket_fd, Message *msg);

#endif // !NETWORK_H
