#include "../include/network.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace velum {

int setup_server(int port) {
  int server_fd;
  struct sockaddr_in address;
  int opt = 1;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("[ERROR] Creating socket failed");
    exit(EXIT_FAILURE);
  }
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("[ERROR] setsockopt");
    exit(EXIT_FAILURE);
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("[ERROR] Bind failed");
    exit(EXIT_FAILURE);
  }
  if (listen(server_fd, 3) < 0) {
    perror("[ERROR] Listen failed");
    exit(EXIT_FAILURE);
  }
  return server_fd;
}

int connect_to_peer(int peer_id, const char *ip) {
  int sock = 0;
  struct sockaddr_in serv_addr;
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(BASE_PORT + peer_id);

  if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    return -1;

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    close(sock);
    return -1;
  }
  return sock;
}

void send_message(int socket_fd, Message *msg) {
  send(socket_fd, msg, sizeof(Message), 0);
}

int setup_udp_broadcast() {
  int sock;
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return -1;

  int broadcast = 1;
  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(DISCOVERY_PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("UDP Bind failed");
    return -1;
  }
  return sock;
}

void send_udp_beacon(int sock, int my_id, int my_tcp_port) {
  struct sockaddr_in broadcast_addr;
  memset(&broadcast_addr, 0, sizeof(broadcast_addr));
  broadcast_addr.sin_family = AF_INET;
  broadcast_addr.sin_port = htons(DISCOVERY_PORT);
  broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

  Beacon b;
  b.magic = 0xCAFEBABE;
  b.node_id = my_id;
  b.tcp_port = my_tcp_port;

  sendto(sock, &b, sizeof(b), 0, (struct sockaddr *)&broadcast_addr,
         sizeof(broadcast_addr));
}

} // namespace velum
