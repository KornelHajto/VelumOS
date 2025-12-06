#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"
#include <cstdint>
#include <netinet/in.h>

namespace velum {

constexpr int BASE_PORT = 8000;
constexpr int MAX_PEERS = 10;

int setup_server(int port);

int connect_to_peer(int peer_id, const char *ip);

void send_message(int socket_fd, Message *msg);

} // namespace velum

#endif // !NETWORK_H
