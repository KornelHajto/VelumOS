#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../include/network.h"
#include "../include/protocol.h"

using namespace velum;

void handle_message(int my_id, Message *msg);

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <Node ID (1-3)>\n", argv[0]);
    return 1;
  }

  int my_id = atoi(argv[1]);
  int my_port = BASE_PORT + my_id;

  int peer_sockets[MAX_PEERS];
  for (int i = 0; i < MAX_PEERS; i++)
    peer_sockets[i] = 0;

  int inbound_sockets[MAX_PEERS];
  for (int i = 0; i < MAX_PEERS; i++)
    inbound_sockets[i] = 0;

  int server_fd = setup_server(my_port);
  printf("Node %d listening on port %d...\n", my_id, my_port);

  for (int i = 1; i <= 3; i++) {
    if (i == my_id)
      continue;

    int sock = connect_to_peer(i, "127.0.0.1");
    if (sock > 0) {
      printf("[Net] Connected to Node %d (Outbound)\n", i);
      peer_sockets[i] = sock;
    }
  }

  fd_set readfds;
  int max_sd, sd, activity;
  Message msg_buffer;

  printf(">>> System Ready. Press [ENTER] to broadcast a Status Report. <<<\n");

  while (1) {
    FD_ZERO(&readfds);

    FD_SET(server_fd, &readfds);
    max_sd = server_fd;

    FD_SET(STDIN_FILENO, &readfds);
    if (STDIN_FILENO > max_sd)
      max_sd = STDIN_FILENO;

    for (int i = 1; i < MAX_PEERS; i++) {
      sd = peer_sockets[i];
      if (sd > 0) {
        FD_SET(sd, &readfds);
        if (sd > max_sd)
          max_sd = sd;
      }
    }

    for (int i = 0; i < MAX_PEERS; i++) {
      sd = inbound_sockets[i];
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

      int added = 0;
      for (int i = 0; i < MAX_PEERS; i++) {
        if (inbound_sockets[i] == 0) {
          inbound_sockets[i] = new_socket;
          printf("[Net] New neighbor connected (Socket %d)\n", new_socket);
          added = 1;
          break;
        }
      }
      if (!added) {
        printf("[Net] Too many connections, rejecting.\n");
        close(new_socket);
      }
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      char dummy[128];
      read(STDIN_FILENO, dummy, 128);

      printf("[Sim] Broadcasting Status Report...\n");

      // --- CREATE THE STATUS REPORT ---
      NodeStatus status;
      status.cpu_load = rand() % 100;            // Random 0-99%
      status.ram_free_kb = 100 + (rand() % 200); // Random 100-300 KB
      status.task_queue_len = rand() % 10;       // Random tasks

      Message broadcast_msg;
      broadcast_msg.sender_id = my_id;
      broadcast_msg.type = MsgType::STATUS_REPORT;

      std::memcpy(broadcast_msg.payload, &status, sizeof(NodeStatus));

      for (int i = 1; i < MAX_PEERS; i++) {
        if (peer_sockets[i] > 0) {
          send_message(peer_sockets[i], &broadcast_msg);
        }
      }
      for (int i = 0; i < MAX_PEERS; i++) {
        if (inbound_sockets[i] > 0) {
          send_message(inbound_sockets[i], &broadcast_msg);
        }
      }
    }

    for (int i = 1; i < MAX_PEERS; i++) {
      sd = peer_sockets[i];
      if (sd > 0 && FD_ISSET(sd, &readfds)) {
        int valread = read(sd, &msg_buffer, sizeof(Message));
        if (valread == 0) {
          close(sd);
          peer_sockets[i] = 0;
          printf("[Net] Node %d disconnected\n", i);
        } else {
          handle_message(my_id, &msg_buffer);
        }
      }
    }

    for (int i = 0; i < MAX_PEERS; i++) {
      sd = inbound_sockets[i];
      if (sd > 0 && FD_ISSET(sd, &readfds)) {
        int valread = read(sd, &msg_buffer, sizeof(Message));
        if (valread == 0) {
          close(sd);
          inbound_sockets[i] = 0;
          printf("[Net] Inbound neighbor disconnected\n");
        } else {
          handle_message(my_id, &msg_buffer);
        }
      }
    }
  }
  return 0;
}
