#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "../include/network.h"
#include "../include/protocol.h"

void handle_message(int my_id, Message *msg);

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <Node ID (1-3)>\n", argv[0]);
    return 1;
  }

  int my_id = atoi(argv[1]);
  int my_port = BASE_PORT + my_id;

  int peer_sockets[MAX_PEERS] = {0};

  int server_fd = setup_server(my_port);
  printf("Node %d listening on port %d...\n", my_id, my_port);

  for (int i = 1; i <= 3; i++) {
    if (i == my_id)
      continue;
    int sock = connect_to_peer(i, "127.0.0.1");
    if (sock > 0) {
      printf("Connected to Node %d\n", i);
      peer_sockets[i] = sock;
    }
  }

  fd_set readfds;
  int max_sd, sd, activity;
  Message msg_buffer;

  while (1) {
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    max_sd = server_fd;

    for (int i = 1; i < MAX_PEERS; i++) {
      sd = peer_sockets[i];
      if (sd > 0) {
        FD_SET(sd, &readfds);
        if (sd > max_sd)
          max_sd = sd;
      }
    }

    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR)) {
      printf("select error");
    }

    if (FD_ISSET(server_fd, &readfds)) {
      int new_socket;
      if ((new_socket = accept(server_fd, NULL, NULL)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
      }
      printf("New Incoming Connection detected.\n");
    }

    for (int i = 1; i < MAX_PEERS; i++) {
      sd = peer_sockets[i];
      if (sd > 0 && FD_ISSET(sd, &readfds)) {
        int valread = read(sd, &msg_buffer, sizeof(Message));
        if (valread == 0) {
          close(sd);
          peer_sockets[i] = 0;
          printf("Node %d disconnected\n", i);
        } else {
          handle_message(my_id, &msg_buffer);
        }
      }
    }
  }
  return 0;
}
