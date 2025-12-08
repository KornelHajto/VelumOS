#include "../common/protocol.h"
#include "../include/m3_env.h"
#include "../include/network.h"
#include "../include/wasm3.h"

#include <algorithm>
#include <arpa/inet.h>
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

struct velum_config_t {
  int node_id;
  int port;
};

static int my_node_id = 0;
static int server_fd = 0;
static int udp_fd = 0;
static int peer_sockets[MAX_PEERS];
static int inbound_sockets[MAX_PEERS];
static int socket_to_id[1024];
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

std::vector<int> find_all_workers(int my_id) {
  std::vector<int> workers;
  for (int i = 1; i < MAX_PEERS; i++) {
    if (i == my_id)
      continue;
    if (peer_sockets[i] > 0)
      workers.push_back(i);
  }
  for (int i = 0; i < MAX_PEERS; i++) {
    int fd = inbound_sockets[i];
    if (fd > 0) {
      int remote_id = socket_to_id[fd];
      if (remote_id > 0 && remote_id != my_id) {
        bool exists = false;
        for (int w : workers)
          if (w == remote_id)
            exists = true;
        if (!exists)
          workers.push_back(remote_id);
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

int execute_wasm_in_sandbox(const uint8_t *binary, uint32_t size,
                            const char *func_name, int param) {
  printf("📦 [Wasm3] Initializing Sandbox for '%s' (%d bytes)...\n", func_name,
         size);

  IM3Environment env = m3_NewEnvironment();
  if (!env)
    return -1;

  IM3Runtime runtime = m3_NewRuntime(env, 64 * 1024, NULL);
  if (!runtime) {
    m3_FreeEnvironment(env);
    return -1;
  }

  IM3Module module;
  if (m3_ParseModule(env, &module, binary, size) != m3Err_none) {
    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    return -2;
  }

  if (m3_LoadModule(runtime, module) != m3Err_none) {
    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    return -3;
  }

  IM3Function func;
  if (m3_FindFunction(&func, runtime, func_name) != m3Err_none) {
    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    return -4;
  }

  char arg_str[16];
  sprintf(arg_str, "%d", param);
  const char *args[1] = {arg_str};

  if (m3_Call(func, 1, (const void **)args) != m3Err_none) {
    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    return -5;
  }

  int result = 0;
  uint64_t val = 0;
  const void *rets[] = {&val};

  if (m3_GetResults(func, 1, rets) == m3Err_none) {
    result = (int)val;
  }

  printf("✅ [Wasm3] Execution Success! Result: %d\n", result);
  m3_FreeRuntime(runtime);
  m3_FreeEnvironment(env);
  return result;
}

void velum_engine_loop() {
  fd_set readfds;
  Message msg_buffer;
  long long last_heartbeat = current_timestamp();
  long long last_beacon = current_timestamp();

  for (int i = 0; i < MAX_PEERS; i++) {
    peer_sockets[i] = 0;
    inbound_sockets[i] = 0;
  }
  for (int i = 0; i < 1024; i++)
    socket_to_id[i] = -1;

  udp_fd = setup_udp_broadcast();
  if (udp_fd > 0)
    printf("[VelumOS] UDP Discovery Active on Port %d\n", DISCOVERY_PORT);

  printf("[VelumOS] Kernel Started. Wasm3 Ready.\n");

  while (running) {
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    int max_sd = server_fd;

    if (udp_fd > 0) {
      FD_SET(udp_fd, &readfds);
      if (udp_fd > max_sd)
        max_sd = udp_fd;
    }

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

    if (now - last_beacon > 3000) {
      last_beacon = now;
      if (udp_fd > 0)
        send_udp_beacon(udp_fd, my_node_id, 8000 + my_node_id);
    }

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

    if (udp_fd > 0 && FD_ISSET(udp_fd, &readfds)) {
      struct sockaddr_in sender_addr;
      socklen_t addr_len = sizeof(sender_addr);
      Beacon b;
      int len = recvfrom(udp_fd, &b, sizeof(b), 0,
                         (struct sockaddr *)&sender_addr, &addr_len);

      if (len == sizeof(Beacon) && b.magic == 0xCAFEBABE) {
        if (b.node_id != my_node_id && b.node_id < MAX_PEERS) {
          if (peer_sockets[b.node_id] == 0) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sender_addr.sin_addr), ip_str,
                      INET_ADDRSTRLEN);

            printf("[Discovery] Found Node %d at %s. Connecting...\n",
                   b.node_id, ip_str);
            int sock = connect_to_peer(b.node_id, ip_str);
            if (sock > 0) {
              peer_sockets[b.node_id] = sock;
              socket_to_id[sock] = b.node_id;
            }
          }
        }
      }
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

    auto handle_sock = [&](int &sock, int i, bool inbound) {
      if (sock > 0 && FD_ISSET(sock, &readfds)) {
        int valread = read(sock, &msg_buffer, sizeof(Message));
        if (valread <= 0) {
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

              if (h->op_code == TaskOp::COMPUTE_PI ||
                  h->op_code == TaskOp::FIND_PRIMES) {
                if (args->iterations > 0) {
                  printf("⚠️ [Recovery] Rescheduling chunk of %d iters for Job "
                         "%d\n",
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
          }
        } else {
          socket_to_id[sock] = msg_buffer.sender_id;

          if (msg_buffer.type == MsgType::TASK_REQUEST) {
            TaskHeader *h = (TaskHeader *)msg_buffer.payload;
            if (h->op_code == TaskOp::EXECUTE_WASM) {
              WasmArgs *args =
                  (WasmArgs *)(msg_buffer.payload + sizeof(TaskHeader));
              uint8_t *binary_ptr =
                  msg_buffer.payload + sizeof(TaskHeader) + sizeof(WasmArgs);
              printf("[Kernel] Received Wasm Job %d. Function: %s\n", h->job_id,
                     args->func_name);
              int res_val = execute_wasm_in_sandbox(
                  binary_ptr, args->binary_size, args->func_name, args->param);
              TaskResult res;
              res.job_id = h->job_id;
              res.value = res_val;
              res.count = 1;
              Message reply;
              reply.sender_id = my_node_id;
              reply.type = MsgType::TASK_RESULT;
              std::memcpy(reply.payload, &res, sizeof(TaskResult));
              int reply_sock = -1;
              if (msg_buffer.sender_id < MAX_PEERS &&
                  peer_sockets[msg_buffer.sender_id] > 0)
                reply_sock = peer_sockets[msg_buffer.sender_id];
              if (reply_sock != -1)
                send_message(reply_sock, &reply);
            } else {
              handle_message(my_node_id, &msg_buffer, sock);
            }
          } else if (msg_buffer.type == MsgType::TASK_RESULT ||
                     msg_buffer.type == MsgType::TASK_PROGRESS) {
            std::lock_guard<std::mutex> lock(api_mutex);
            TaskResult *res = (TaskResult *)msg_buffer.payload;

            if (active_jobs.count(res->job_id)) {
              active_jobs[res->job_id].current_done += res->count;
              active_jobs[res->job_id].accumulated_value += res->value;
            }

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

            if (msg_buffer.type == MsgType::TASK_RESULT) {
              if (active_jobs.count(res->job_id)) {
                JobState js = active_jobs[res->job_id];
                if (js.current_done >= js.total_needed) {
                  double pi = 4.0 * ((double)js.accumulated_value /
                                     (double)js.current_done);
                  printf("✅ [Callback] Job %d FINISHED! PI = %f\n",
                         res->job_id, pi);
                  fflush(stdout);
                  active_jobs.erase(res->job_id);
                }
              } else {
                printf("✅ [Callback] Job %d Returned: %d\n", res->job_id,
                       res->value);
                fflush(stdout);
              }
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

void velum_init(int id, int port) {
  my_node_id = id;
  server_fd = setup_server(port);
  printf("[VelumOS] Initialized Node %d on Port %d\n", id, port);
  background_thread = std::thread(velum_engine_loop);
  background_thread.detach();
}

void velum_spawn_wasm(const char *filepath, const char *func, int param) {
  std::lock_guard<std::mutex> lock(api_mutex);
  FILE *f = fopen(filepath, "rb");
  if (!f) {
    printf("[VelumOS] Could not open %s\n", filepath);
    return;
  }
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (fsize > 2000) {
    printf("[VelumOS] Wasm file too big (Max 2KB)\n");
    fclose(f);
    return;
  }
  uint8_t buffer[2048];
  fread(buffer, 1, fsize, f);
  fclose(f);
  int winner = find_best_node(my_node_id);
  if (winner == -1) {
    printf("[VelumOS] No workers.\n");
    return;
  }
  TaskHeader header;
  header.op_code = TaskOp::EXECUTE_WASM;
  header.job_id = rand() % 9999;
  WasmArgs args;
  args.binary_size = (uint32_t)fsize;
  args.param = param;
  strncpy(args.func_name, func, 31);
  Message msg;
  msg.sender_id = my_node_id;
  msg.type = MsgType::TASK_REQUEST;
  uint8_t *ptr = msg.payload;
  std::memcpy(ptr, &header, sizeof(TaskHeader));
  ptr += sizeof(TaskHeader);
  std::memcpy(ptr, &args, sizeof(WasmArgs));
  ptr += sizeof(WasmArgs);
  std::memcpy(ptr, buffer, fsize);
  int target = -1;
  if (winner < MAX_PEERS && peer_sockets[winner] > 0)
    target = peer_sockets[winner];
  else
    for (int i = 0; i < MAX_PEERS; i++)
      if (inbound_sockets[i] > 0 && socket_to_id[inbound_sockets[i]] == winner)
        target = inbound_sockets[i];
  if (target != -1) {
    send_message(target, &msg);
    mark_node_busy(winner);
    printf("[VelumOS] Sent Wasm Job %d ('%s') to Node %d\n", header.job_id,
           func, winner);
  }
}

void velum_spawn(TaskOp op, uint32_t work_amount) {
  std::lock_guard<std::mutex> lock(api_mutex);
  std::vector<int> workers = find_all_workers(my_node_id);
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
