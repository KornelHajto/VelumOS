#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "../include/network.h"
#include "../include/protocol.h"
#include "../include/raft.h"

#define MAX_CLIENTS 10

// Global peer sockets for the callback to access
static int peer_sockets[MAX_PEERS] = {0}; // 1-based index (1, 2, 3)

// List of sockets that we accepted but don't know who they are yet
static int client_sockets[MAX_CLIENTS] = {0};

// Timestamps for retries
static uint64_t last_connect_attempt = 0;

// Helper to get current time in milliseconds is already in raft.h

// Wrapper for Raft to send messages
int send_msg_wrapper(int target_id, Message *msg) {
  if (target_id < 0 || target_id >= MAX_PEERS)
    return -1;
  int sock = peer_sockets[target_id];
  if (sock > 0) {
    send_message(sock, msg);
    return 0;
  }
  return -1;
}

void try_connect_peers(int my_id) {
  uint64_t now = get_current_time_ms();
  if (now - last_connect_attempt < 1000)
    return; // Retry every 1s
  last_connect_attempt = now;

  for (int i = 1; i <= 3; i++) {
    if (i == my_id)
      continue;
    if (peer_sockets[i] == 0) {
      // Try to connect
      int sock = connect_to_peer(i, "127.0.0.1");
      if (sock > 0) {
        printf("[Net] Connected to Node %d (Outbound)\n", i);
        peer_sockets[i] = sock;
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <Node ID (1-3)>\n", argv[0]);
    return 1;
  }

  int my_id = atoi(argv[1]);
  int my_port = BASE_PORT + my_id;

  // Seed RNG
  srand(time(NULL) + my_id);

  // Initialize Raft
  RaftNode my_raft;
  raft_init(&my_raft, my_id, NUM_NODES);

  int server_fd = setup_server(my_port);
  printf("Node %d listening on port %d...\n", my_id, my_port);

  // Set server socket to non-blocking to avoid hanging?
  // actually select handles that.

  fd_set readfds;
  int max_sd, sd, activity;
  Message msg_buffer;

  while (1) {
    // 1. Try to connect to missing peers
    try_connect_peers(my_id);

    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    max_sd = server_fd;

    // Add known peers
    for (int i = 1; i <= NUM_NODES; i++) {
      sd = peer_sockets[i];
      if (sd > 0) {
        FD_SET(sd, &readfds);
        if (sd > max_sd)
          max_sd = sd;
      }
    }

    // Add unknown clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
      sd = client_sockets[i];
      if (sd > 0) {
        FD_SET(sd, &readfds);
        if (sd > max_sd)
          max_sd = sd;
      }
    }

    // Timeout 50ms
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;

    activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

    if ((activity < 0) && (errno != EINTR)) {
      printf("select error");
    }

    // Raft Tick
    raft_tick(&my_raft, send_msg_wrapper);

    // Incoming Connection
    if (FD_ISSET(server_fd, &readfds)) {
      int new_socket;
      if ((new_socket = accept(server_fd, NULL, NULL)) < 0) {
        perror("accept");
        // Don't exit, just ignore
      } else {
        // printf("[Net] New Incoming Connection\n");
        // Add to client list
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (client_sockets[i] == 0) {
            client_sockets[i] = new_socket;
            break;
          }
        }
      }
    }

    // Check Unknown Clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
      sd = client_sockets[i];
      if (sd > 0 && FD_ISSET(sd, &readfds)) {
        int valread = read(sd, &msg_buffer, sizeof(Message));
        if (valread == 0) {
          close(sd);
          client_sockets[i] = 0;
        } else {
          // We have a message! Now we know who it is.
          int sender = msg_buffer.sender_id;
          if (sender > 0 && sender < MAX_PEERS) {
            if (peer_sockets[sender] == 0) {
              printf("[Net] Identified Node %d via Inbound\n", sender);
              peer_sockets[sender] = sd;
              client_sockets[i] = 0; // Remove from temp list
              // Process the message
              raft_handle_msg(&my_raft, &msg_buffer, send_msg_wrapper);
            } else {
              // We already have a connection?
              // Check if this is a new one replacing old?
              // For now, assume existing is good, but process message anyway
              // Actually, if we accepted a new one, maybe the old one is dead.
              // But for simplicity, just process message.
              raft_handle_msg(&my_raft, &msg_buffer, send_msg_wrapper);

              // If we assume this socket is better, we'd swap.
              // But usually we keep the authenticated one.
              // Since we removed it from client_sockets, we must track it or
              // close it. Wait, if I don't assign it to peer_sockets, I lose
              // reference to it in next loop if I zero it here. If
              // peer_sockets[sender] != sd, we have two sockets for same peer.
              // Let's just use this one for now to process, but keep it in
              // client_sockets? No, that will spam. Simple strategy: If we have
              // a peer socket, close this new one? No, maybe the old one broke.
              // Let's Update peer_sockets if different
              if (peer_sockets[sender] != sd) {
                close(peer_sockets[sender]);
                peer_sockets[sender] = sd;
                client_sockets[i] = 0;
              }
            }
          }
        }
      }
    }

    // Check Known Peers
    for (int i = 1; i < MAX_PEERS; i++) {
      sd = peer_sockets[i];
      if (sd > 0 && FD_ISSET(sd, &readfds)) {
        int valread = read(sd, &msg_buffer, sizeof(Message));
        if (valread == 0) {
          close(sd);
          peer_sockets[i] = 0;
          printf("[Net] Node %d disconnected\n", i);
        } else {
          raft_handle_msg(&my_raft, &msg_buffer, send_msg_wrapper);
        }
      }
    }
  }
  return 0;
}
