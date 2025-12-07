#include "../common/protocol.h"
#include "../include/network.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <sys/select.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace velum;

// --- CONFIGURATION ---
struct velum_config_t {
  int node_id;
  int port;
};

// --- INTERNAL STATE ---
static int my_node_id = 0;
static int server_fd = 0;
static int peer_sockets[MAX_PEERS];    // Index = Node ID
static int inbound_sockets[MAX_PEERS]; // Index = Just a slot (0, 1, 2...)
static int socket_to_id[1024];         // Map FD -> Node ID
static std::atomic<bool> running(true);
static std::thread background_thread;
static std::mutex api_mutex;

static std::map<int, Message> pending_tasks;

struct JobState {
  uint32_t total_needed;
  uint32_t current_done;
  int64_t accumulated_value;
};
static std::map<uint32_t, JobState> active_jobs;

extern void handle_message(int my_id, Message *msg, int src_socket);
extern int find_best_node(int my_id);
extern void handle_disconnect(int node_id);
extern void mark_node_busy(int node_id);

// --- NEW HELPER: Find ALL valid workers (FIXED) ---
std::vector<int> find_all_workers(int my_id) {
  std::vector<int> workers;

  // 1. Check Outbound Peers (Index IS the Node ID)
  for (int i = 1; i < MAX_PEERS; i++) {
    if (i == my_id)
      continue;
    if (peer_sockets[i] > 0) {
      workers.push_back(i);
    }
  }

  // 2. Check Inbound Sockets (Index is arbitrary, need to look up ID)
  for (int i = 0; i < MAX_PEERS; i++) {
    int fd = inbound_sockets[i];
    if (fd > 0) {
      int remote_id = socket_to_id[fd]; // Look up who this is
      if (remote_id > 0 && remote_id != my_id) {
        // Avoid duplicates (if we are connected both ways)
        bool already_added = false;
        for (int w : workers)
          if (w == remote_id)
            already_added = true;

        if (!already_added) {
          workers.push_back(remote_id);
        }
      }
    }
  }
  return workers;
}

long long current_timestamp() {
  struct timeval te;
  gettimeofday(&te, NULL);
  return te.tv_sec * 1000LL + te.tv_usec / 1000;
}

// --- ENGINE LOOP ---
void velum_engine_loop() {
  fd_set readfds;
  Message msg_buffer;
  long long last_heartbeat = current_timestamp();

  // Init arrays
  for (int i = 0; i < MAX_PEERS; i++) {
    peer_sockets[i] = 0;
    inbound_sockets[i] = 0;
  }
  for (int i = 0; i < 1024; i++)
    socket_to_id[i] = -1;

  // Connect Loop
  for (int i = 1; i < MAX_PEERS; i++) {
    if (i == my_node_id)
      continue;
    int sock = connect_to_peer(i, "127.0.0.1");
    if (sock > 0) {
      peer_sockets[i] = sock;
      socket_to_id[sock] = i;
    }
  }
  printf("[VelumOS] Kernel Started. Mesh Active.\n");

  while (running) {
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    int max_sd = server_fd;

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

    // Heartbeat
    long long now = current_timestamp();
    if (now - last_heartbeat > 500) {
      last_heartbeat = now;
      NodeStatus status;
      status.cpu_load = rand() % 100;
      status.ram_free_kb = 100;
      status.task_queue_len = rand() % 10;
      Message b_msg;
      b_msg.sender_id = my_node_id;
      b_msg.type = MsgType::STATUS_REPORT;
      std::memcpy(b_msg.payload, &status, sizeof(NodeStatus));
      for (int i = 1; i < MAX_PEERS; i++)
        if (peer_sockets[i] > 0)
          send_message(peer_sockets[i], &b_msg);
      for (int i = 0; i < MAX_PEERS; i++)
        if (inbound_sockets[i] > 0)
          send_message(inbound_sockets[i], &b_msg);
    }

    // Connections
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

    // Messages
    auto handle_sock = [&](int &sock, int i, bool inbound) {
      if (sock > 0 && FD_ISSET(sock, &readfds)) {
        int valread = read(sock, &msg_buffer, sizeof(Message));
        if (valread <= 0) {
          // CRASH HANDLER
          int dead_node = socket_to_id[sock];
          close(sock);
          sock = 0;
          socket_to_id[sock] = -1;
          if (dead_node != -1) {
            std::lock_guard<std::mutex> lock(api_mutex);
            printf("🚨 [Kernel] Node %d died!\n", dead_node);
            handle_disconnect(dead_node);

            if (pending_tasks.count(dead_node)) {
              Message lost_msg = pending_tasks[dead_node];
              pending_tasks.erase(dead_node);

              TaskHeader *h = (TaskHeader *)lost_msg.payload;
              ComputeArgs *args =
                  (ComputeArgs *)(lost_msg.payload + sizeof(TaskHeader));

              if (args->iterations > 0) {
                printf(
                    "⚠️ [Recovery] Rescheduling chunk of %d iters for Job %d\n",
                    args->iterations, h->job_id);
                int new_winner = find_best_node(my_node_id);
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
                    pending_tasks[new_winner] = lost_msg;
                  }
                }
              }
            }
          }
        } else {
          socket_to_id[sock] = msg_buffer.sender_id;
          if (msg_buffer.type == MsgType::TASK_PROGRESS ||
              msg_buffer.type == MsgType::TASK_RESULT) {
            std::lock_guard<std::mutex> lock(api_mutex);
            TaskResult *res = (TaskResult *)msg_buffer.payload;

            // Accumulate
            if (active_jobs.count(res->job_id)) {
              active_jobs[res->job_id].current_done += res->count;
              active_jobs[res->job_id].accumulated_value += res->value;
            }

            // Update Pending Ledger
            if (pending_tasks.count(msg_buffer.sender_id)) {
              Message *p_msg = &pending_tasks[msg_buffer.sender_id];
              ComputeArgs *p_args =
                  (ComputeArgs *)(p_msg->payload + sizeof(TaskHeader));
              if (p_args->iterations > res->count)
                p_args->iterations -= res->count;
              else
                p_args->iterations = 0;

              if (msg_buffer.type == MsgType::TASK_RESULT)
                pending_tasks.erase(msg_buffer.sender_id);
            }

            // Check Global Completion
            JobState js = active_jobs[res->job_id];
            if (js.current_done >= js.total_needed) {
              double pi = 4.0 * ((double)js.accumulated_value /
                                 (double)js.current_done);
              printf("✅ [Callback] Job %d FINISHED! PI = %f\n", res->job_id,
                     pi);
              active_jobs.erase(res->job_id);
            }
          } else {
            handle_message(my_node_id, &msg_buffer, sock);
          }
        }
      }
    };
    for (int i = 1; i < MAX_PEERS; i++)
      handle_sock(peer_sockets[i], i, false);
    for (int i = 0; i < MAX_PEERS; i++)
      handle_sock(inbound_sockets[i], i, true);
  }
}

// --- PUBLIC API ---

void velum_init(int id, int port) {
  my_node_id = id;
  server_fd = setup_server(port);
  printf("[VelumOS] Initialized Node %d on Port %d\n", id, port);
  background_thread = std::thread(velum_engine_loop);
  background_thread.detach();
}

void velum_spawn(TaskOp op, uint32_t work_amount) {
  std::lock_guard<std::mutex> lock(api_mutex);

  // 1. Find ALL workers using the FIXED logic
  std::vector<int> workers = find_all_workers(my_node_id);

  // Safety check: if system just started, wait a tiny bit for heartbeats
  if (workers.empty()) {
    printf("[VelumOS] No workers found yet. Waiting 1s...\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    workers = find_all_workers(my_node_id);
  }

  if (workers.empty()) {
    printf("[VelumOS] Error: Still no workers found.\n");
    return;
  }

  uint32_t job_id = rand() % 9999;
  active_jobs[job_id] = {work_amount, 0, 0};

  // 2. SCATTER
  int num_workers = workers.size();
  uint32_t chunk_per_node = work_amount / num_workers;
  uint32_t remainder = work_amount % num_workers;

  printf("[VelumOS] Scattering Job %d: %d items across %d workers...\n", job_id,
         work_amount, num_workers);

  for (int i = 0; i < num_workers; i++) {
    int worker_id = workers[i];
    uint32_t my_chunk = chunk_per_node;
    if (i == num_workers - 1)
      my_chunk += remainder;

    TaskHeader header;
    header.op_code = op;
    header.job_id = job_id;
    ComputeArgs args;
    args.iterations = my_chunk;

    Message task_msg;
    task_msg.sender_id = my_node_id;
    task_msg.type = MsgType::TASK_REQUEST;
    std::memcpy(task_msg.payload, &header, sizeof(TaskHeader));
    std::memcpy(task_msg.payload + sizeof(TaskHeader), &args,
                sizeof(ComputeArgs));

    int target = -1;
    if (worker_id < MAX_PEERS && peer_sockets[worker_id] > 0)
      target = peer_sockets[worker_id];
    else
      for (int k = 0; k < MAX_PEERS; k++)
        if (inbound_sockets[k] > 0 &&
            socket_to_id[inbound_sockets[k]] == worker_id)
          target = inbound_sockets[k];

    if (target != -1) {
      send_message(target, &task_msg);
      mark_node_busy(worker_id);
      pending_tasks[worker_id] = task_msg;
      printf("   -> Sent chunk (%d) to Node %d\n", my_chunk, worker_id);
    }
  }
}
