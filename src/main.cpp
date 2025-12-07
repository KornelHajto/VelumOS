#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <map>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../common/protocol.h"
#include "../include/network.h"

using namespace velum;

void handle_message(int my_id, Message *msg, int src_socket);
int find_best_node(int my_id);
void handle_disconnect(int node_id);
void mark_node_busy(int node_id);

int socket_to_id[1024];

// TRACKING
std::map<int, Message> pending_tasks;

struct JobState {
  uint32_t total_needed;
  uint32_t current_done;
  int64_t accumulated_value;
};
std::map<uint32_t, JobState> active_jobs;

long long current_timestamp() {
  struct timeval te;
  gettimeofday(&te, NULL);
  long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
  return milliseconds;
}

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  srand(time(NULL));

  if (argc != 2)
    return 1;
  int my_id = atoi(argv[1]);
  int my_port = BASE_PORT + my_id;

  for (int i = 0; i < 1024; i++)
    socket_to_id[i] = -1;
  int peer_sockets[MAX_PEERS];
  for (int i = 0; i < MAX_PEERS; i++)
    peer_sockets[i] = 0;
  int inbound_sockets[MAX_PEERS];
  for (int i = 0; i < MAX_PEERS; i++)
    inbound_sockets[i] = 0;
  int server_fd = setup_server(my_port);
  printf("Node %d listening on port %d...\n", my_id, my_port);
  for (int i = 1; i < MAX_PEERS; i++) {
    if (i == my_id)
      continue;
    int sock = connect_to_peer(i, "127.0.0.1");
    if (sock > 0) {
      peer_sockets[i] = sock;
      socket_to_id[sock] = i;
    }
  }

  fd_set readfds;
  Message msg_buffer;
  long long last_heartbeat = current_timestamp();

  printf(">>> Ready. Checkpointing Enabled.\n");

  while (1) {
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    int max_sd = server_fd;
    FD_SET(STDIN_FILENO, &readfds);
    if (STDIN_FILENO > max_sd)
      max_sd = STDIN_FILENO;
    for (int i = 1; i < MAX_PEERS; i++)
      if (peer_sockets[i] > 0) {
        FD_SET(peer_sockets[i], &readfds);
        if (peer_sockets[i] > max_sd)
          max_sd = peer_sockets[i];
      }
    for (int i = 0; i < MAX_PEERS; i++)
      if (inbound_sockets[i] > 0) {
        FD_SET(inbound_sockets[i], &readfds);
        if (inbound_sockets[i] > max_sd)
          max_sd = inbound_sockets[i];
      }
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    select(max_sd + 1, &readfds, NULL, NULL, &tv);

    long long now = current_timestamp();
    if (now - last_heartbeat > 500) {
      last_heartbeat = now;
      NodeStatus status;
      status.cpu_load = rand() % 100;
      status.ram_free_kb = 100 + (rand() % 200);
      status.task_queue_len = rand() % 10;
      Message b_msg;
      b_msg.sender_id = my_id;
      b_msg.type = MsgType::STATUS_REPORT;
      std::memcpy(b_msg.payload, &status, sizeof(NodeStatus));
      for (int i = 1; i < MAX_PEERS; i++)
        if (peer_sockets[i] > 0)
          send_message(peer_sockets[i], &b_msg);
      for (int i = 0; i < MAX_PEERS; i++)
        if (inbound_sockets[i] > 0)
          send_message(inbound_sockets[i], &b_msg);
    }

    if (FD_ISSET(server_fd, &readfds)) {
      int ns = accept(server_fd, NULL, NULL);
      int added = 0;
      for (int i = 0; i < MAX_PEERS; i++)
        if (inbound_sockets[i] == 0) {
          inbound_sockets[i] = ns;
          added = 1;
          break;
        }
      if (!added)
        close(ns);
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      char buffer[256];
      int nbytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
      if (nbytes > 0) {
        buffer[nbytes] = '\0';
        char cmd[16];
        int val1 = 0;
        int args_found = sscanf(buffer, "%s %d", cmd, &val1);

        if (args_found >= 1) {
          TaskHeader header;
          header.job_id = rand() % 9999;
          bool valid = true;
          uint8_t temp_payload[200];

          if (strcmp(cmd, "pi") == 0) {
            header.op_code = TaskOp::COMPUTE_PI;
            ComputeArgs args;
            args.iterations = val1;
            std::memcpy(temp_payload, &args, sizeof(ComputeArgs));
            active_jobs[header.job_id] = {(uint32_t)val1, 0, 0};
            printf("[Cmd] Job %d Started: Pi %d iterations\n", header.job_id,
                   val1);
          } else
            valid = false;

          if (valid) {
            int winner = find_best_node(my_id);
            if (winner != -1) {
              Message task_msg;
              task_msg.sender_id = my_id;
              task_msg.type = MsgType::TASK_REQUEST;
              std::memcpy(task_msg.payload, &header, sizeof(TaskHeader));
              std::memcpy(task_msg.payload + sizeof(TaskHeader), temp_payload,
                          128);

              int target = -1;
              if (winner < MAX_PEERS && peer_sockets[winner] > 0)
                target = peer_sockets[winner];
              else
                for (int i = 0; i < MAX_PEERS; i++)
                  if (inbound_sockets[i] > 0 &&
                      socket_to_id[inbound_sockets[i]] == winner)
                    target = inbound_sockets[i];

              if (target != -1) {
                send_message(target, &task_msg);
                mark_node_busy(winner);
                pending_tasks[winner] = task_msg;
                printf("[Sim] Sent Job %d to Node %d\n", header.job_id, winner);
              }
            }
          }
        }
      }
    }

    auto handle_sock_activity = [&](int &sock, int i, bool inbound) {
      if (sock > 0 && FD_ISSET(sock, &readfds)) {
        int valread = read(sock, &msg_buffer, sizeof(Message));
        if (valread <= 0) { // Catch 0 (Disconnect) and -1 (Error)
          int dead_node = socket_to_id[sock];
          close(sock);
          sock = 0;
          socket_to_id[sock] = -1;

          if (dead_node != -1) {
            printf("🚨 [CRASH] Node %d died! (Checking recovery...)\n",
                   dead_node);
            handle_disconnect(dead_node);

            // --- DEBUG START ---
            if (pending_tasks.count(dead_node)) {
              printf("✅ [Debug] Found pending task for Node %d in map.\n",
                     dead_node);
            } else {
              printf("❌ [Debug] Pending tasks map is missing Node %d!\n",
                     dead_node);
              printf("   [Debug] Map contents: ");
              if (pending_tasks.empty())
                printf("(Empty)");
              for (auto const &[key, val] : pending_tasks)
                printf("[%d] ", key);
              printf("\n");
            }
            // --- DEBUG END ---

            if (pending_tasks.count(dead_node)) {
              Message lost_msg = pending_tasks[dead_node];
              pending_tasks.erase(dead_node);

              TaskHeader *h = (TaskHeader *)lost_msg.payload;
              ComputeArgs *args =
                  (ComputeArgs *)(lost_msg.payload + sizeof(TaskHeader));

              printf("⚠️ [Recovery] Job %d interrupted. Recovered progress: %d "
                     "done.\n",
                     h->job_id, active_jobs[h->job_id].current_done);
              printf("♻️ [Resume] Rescheduling remaining %d iterations...\n",
                     args->iterations);

              int new_winner = find_best_node(my_id);
              if (new_winner != -1) {
                int new_target = -1;
                if (new_winner < MAX_PEERS && peer_sockets[new_winner] > 0)
                  new_target = peer_sockets[new_winner];
                else
                  for (int k = 0; k < MAX_PEERS; k++)
                    if (inbound_sockets[k] > 0 &&
                        socket_to_id[inbound_sockets[k]] == new_winner)
                      new_target = inbound_sockets[k];

                if (new_target != -1) {
                  send_message(new_target, &lost_msg);
                  mark_node_busy(new_winner);
                  pending_tasks[new_winner] = lost_msg;
                  printf("   -> Sent to Node %d\n", new_winner);
                }
              }
            }
          }
        } else {
          socket_to_id[sock] = msg_buffer.sender_id;

          if (msg_buffer.type == MsgType::TASK_PROGRESS ||
              msg_buffer.type == MsgType::TASK_RESULT) {
            TaskResult *res = (TaskResult *)msg_buffer.payload;

            if (active_jobs.count(res->job_id)) {
              active_jobs[res->job_id].current_done += res->count;
              active_jobs[res->job_id].accumulated_value += res->value;

              printf("📊 [Job %d] Progress: %d / %d (+%d from Node %d)\n",
                     res->job_id, active_jobs[res->job_id].current_done,
                     active_jobs[res->job_id].total_needed, res->count,
                     msg_buffer.sender_id);
            }

            if (pending_tasks.count(msg_buffer.sender_id)) {
              Message *p_msg = &pending_tasks[msg_buffer.sender_id];
              ComputeArgs *p_args =
                  (ComputeArgs *)(p_msg->payload + sizeof(TaskHeader));

              if (p_args->iterations > res->count) {
                p_args->iterations -= res->count;
              } else {
                p_args->iterations = 0;
              }

              if (msg_buffer.type == MsgType::TASK_RESULT) {
                pending_tasks.erase(msg_buffer.sender_id);
              }
            }

            if (msg_buffer.type == MsgType::TASK_RESULT) {
              JobState js = active_jobs[res->job_id];
              if (js.current_done >= js.total_needed) {
                double pi = 4.0 * ((double)js.accumulated_value /
                                   (double)js.current_done);
                printf("✅ [Job %d] FINAL SUCCESS! PI = %f\n", res->job_id, pi);
                active_jobs.erase(res->job_id);
              }
            }
          } else {
            handle_message(my_id, &msg_buffer, sock);
          }
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
