#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime> // For random seed
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../common/protocol.h"
#include "../include/network.h"

using namespace velum;

// Function Prototypes
void handle_message(int my_id, Message *msg, int src_socket);
int find_best_node(int my_id);
void handle_disconnect(int node_id);
void mark_node_busy(int node_id);

int socket_to_id[1024];

long long current_timestamp() {
  struct timeval te;
  gettimeofday(&te, NULL);
  long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
  return milliseconds;
}

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL); // Disable buffering for logs
  srand(time(NULL));    // Seed random number generator

  if (argc != 2) {
    printf("Usage: %s <Node ID (1-%d)>\n", argv[0], MAX_PEERS - 1);
    return 1;
  }

  for (int i = 0; i < 1024; i++)
    socket_to_id[i] = -1;

  int my_id = atoi(argv[1]);
  int my_port = BASE_PORT + my_id;

  // Socket Arrays
  int peer_sockets[MAX_PEERS];
  for (int i = 0; i < MAX_PEERS; i++)
    peer_sockets[i] = 0;

  int inbound_sockets[MAX_PEERS];
  for (int i = 0; i < MAX_PEERS; i++)
    inbound_sockets[i] = 0;

  int server_fd = setup_server(my_port);
  printf("Node %d listening on port %d...\n", my_id, my_port);

  // Connect to peers
  for (int i = 1; i < MAX_PEERS; i++) {
    if (i == my_id)
      continue;
    int sock = connect_to_peer(i, "127.0.0.1");
    if (sock > 0) {
      printf("[Net] Connected to Node %d (Outbound)\n", i);
      peer_sockets[i] = sock;
      socket_to_id[sock] = i;
    }
  }

  fd_set readfds;
  int max_sd, sd, activity;
  Message msg_buffer;
  long long last_heartbeat = current_timestamp();
  long long heartbeat_interval = 2000;

  printf(">>> System Ready. Try these commands:\n");
  printf("    pi 200000    (Calculate Pi)\n");
  printf("    prime 50000  (Find Primes)\n");
  printf("    add 10 20    (Simple Math)\n");

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

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

    long long now = current_timestamp();
    if (now - last_heartbeat > heartbeat_interval) {
      last_heartbeat = now;
      NodeStatus status;
      status.cpu_load = rand() % 100;
      status.ram_free_kb = 100 + (rand() % 200);
      status.task_queue_len = rand() % 10;

      Message broadcast_msg;
      broadcast_msg.sender_id = my_id;
      broadcast_msg.type = MsgType::STATUS_REPORT;
      std::memcpy(broadcast_msg.payload, &status, sizeof(NodeStatus));

      for (int i = 1; i < MAX_PEERS; i++)
        if (peer_sockets[i] > 0)
          send_message(peer_sockets[i], &broadcast_msg);
      for (int i = 0; i < MAX_PEERS; i++)
        if (inbound_sockets[i] > 0)
          send_message(inbound_sockets[i], &broadcast_msg);
    }

    if (FD_ISSET(server_fd, &readfds)) {
      int new_socket;
      if ((new_socket = accept(server_fd, NULL, NULL)) < 0)
        exit(EXIT_FAILURE);
      int added = 0;
      for (int i = 0; i < MAX_PEERS; i++) {
        if (inbound_sockets[i] == 0) {
          inbound_sockets[i] = new_socket;
          printf("[Net] New neighbor connected (Socket %d)\n", new_socket);
          added = 1;
          break;
        }
      }
      if (!added)
        close(new_socket);
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      char buffer[256];
      int nbytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
      if (nbytes > 0) {
        buffer[nbytes] = '\0';
        char cmd[16];
        int val1 = 0, val2 = 0;
        int args_found = sscanf(buffer, "%s %d %d", cmd, &val1, &val2);

        if (args_found >= 1) {
          TaskHeader header;
          header.job_id = rand() % 9999; // Generate Random Job ID
          bool valid = true;
          uint8_t temp_payload[200];

          if (strcmp(cmd, "add") == 0 && args_found >= 3) {
            header.op_code = TaskOp::MATH_ADD;
            MathArgs args = {val1, val2};
            std::memcpy(temp_payload, &args, sizeof(MathArgs));
          } else if (strcmp(cmd, "sub") == 0 && args_found >= 3) {
            header.op_code = TaskOp::MATH_SUB;
            MathArgs args = {val1, val2};
            std::memcpy(temp_payload, &args, sizeof(MathArgs));
          } else if (strcmp(cmd, "pi") == 0 && args_found >= 2) {
            header.op_code = TaskOp::COMPUTE_PI;
            ComputeArgs args;
            args.iterations = val1;
            std::memcpy(temp_payload, &args, sizeof(ComputeArgs));
            printf("[Cmd] Created Job %d: PI Calc (%d iters)\n", header.job_id,
                   val1);
          } else if (strcmp(cmd, "prime") == 0 && args_found >= 2) {
            header.op_code = TaskOp::FIND_PRIMES;
            ComputeArgs args;
            args.iterations = val1;
            std::memcpy(temp_payload, &args, sizeof(ComputeArgs));
            printf("[Cmd] Created Job %d: Find Primes (Limit %d)\n",
                   header.job_id, val1);
          } else {
            valid = false;
          }

          // --- DISPATCH ---
          if (valid) {
            int winner = find_best_node(my_id);
            if (winner != -1) {
              printf("[Sim] Offloading Job %d to Node %d\n", header.job_id,
                     winner);

              Message task_msg;
              task_msg.sender_id = my_id;
              task_msg.type = MsgType::TASK_REQUEST;

              std::memcpy(task_msg.payload, &header, sizeof(TaskHeader));
              std::memcpy(task_msg.payload + sizeof(TaskHeader), temp_payload,
                          128);

              // Find correct socket
              int target = -1;
              if (winner < MAX_PEERS && peer_sockets[winner] > 0)
                target = peer_sockets[winner];
              else {
                for (int i = 0; i < MAX_PEERS; i++) {
                  if (inbound_sockets[i] > 0 &&
                      socket_to_id[inbound_sockets[i]] == winner) {
                    target = inbound_sockets[i];
                    break;
                  }
                }
              }

              if (target != -1) {
                send_message(target, &task_msg);
                mark_node_busy(winner);
              } else
                printf("[Err] Link lost to Node %d\n", winner);
            } else {
              printf("[Sim] No workers available.\n");
            }
          } else {
            printf("[Cmd] Error. Usage:\n  pi <iters>\n  prime <limit>\n  add "
                   "<n> <n>\n");
          }
        }
      }
    }

    // 4. Handle Network Messages
    auto handle_sock_activity = [&](int &sock, int i, bool inbound) {
      if (sock > 0 && FD_ISSET(sock, &readfds)) {
        int valread = read(sock, &msg_buffer, sizeof(Message));
        if (valread == 0) {
          int who = socket_to_id[sock];
          close(sock);
          sock = 0;
          socket_to_id[sock] = -1;
          if (who != -1) {
            handle_disconnect(who);
            printf("[Net] Node %d disconnected\n", who);
          }
        } else {
          socket_to_id[sock] = msg_buffer.sender_id;
          handle_message(my_id, &msg_buffer, sock);
        }
      }
    };

    for (int i = 1; i < MAX_PEERS; i++)
      handle_sock_activity(peer_sockets[i], i, false);
    for (int i = 0; i < MAX_PEERS; i++)
      handle_sock_activity(inbound_sockets[i], i, true);
  }
  return 0;
}
