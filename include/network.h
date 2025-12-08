#ifndef NETWORK_H
#define NETWORK_H

#include "../common/protocol.h"
#include <cstdint>
#include <netinet/in.h>

namespace velum {

constexpr int BASE_PORT = 8000;
constexpr int MAX_PEERS = 16;
constexpr int DISCOVERY_PORT = 8888;

int setup_server(int port);

int connect_to_peer(int peer_id, const char *ip);

void send_message(int socket_fd, Message *msg);

int setup_udp_broadcast();
void send_udp_beacon(int sock, int my_id, int my_tcp_port);

struct Beacon {
  uint32_t magic;
  int node_id;
  int tcp_port;
};
} // namespace velum

#endif // !NETWORK_H
