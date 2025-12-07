#include "../common/protocol.h"
#include "../include/network.h"
#include "../include/user_tasks.h" // <--- CRITICAL: Defines the functions, doesn't implement them
#include <cstdio>
#include <cstring>

using namespace velum;

// Global cluster status storage
NodeStatus cluster_status[MAX_PEERS + 1];

// ==========================================
// MESSAGE HANDLER & DISPATCHER
// ==========================================

void handle_message(int my_id, Message *msg, int src_socket) {
  switch (msg->type) {

  case MsgType::STATUS_REPORT: {
    if (msg->sender_id <= MAX_PEERS) {
      std::memcpy(&cluster_status[msg->sender_id], msg->payload,
                  sizeof(NodeStatus));
    }
    break;
  }

  case MsgType::HEARTBEAT:
    break;

  case MsgType::TASK_REQUEST: {
    const TaskHeader *header = (const TaskHeader *)msg->payload;

    const uint8_t *data_ptr = msg->payload + sizeof(TaskHeader);

    printf("[Job %d] Received Task Request (Op Code: %d)\n", header->job_id,
           (int)header->op_code);

    TaskResult res;
    res.job_id = header->job_id;
    res.value = 0;
    res.count = 0;

    switch (header->op_code) {
    case TaskOp::MATH_ADD: {
      const MathArgs *args = (const MathArgs *)data_ptr;
      res.value = args->a + args->b;
      printf("   -> Executing ADD: %d + %d\n", args->a, args->b);
      break;
    }
    case TaskOp::MATH_SUB: {
      const MathArgs *args = (const MathArgs *)data_ptr;
      res.value = args->a - args->b;
      printf("   -> Executing SUB: %d - %d\n", args->a, args->b);
      break;
    }
    case TaskOp::COMPUTE_PI: {
      const ComputeArgs *args = (const ComputeArgs *)data_ptr;
      printf("   -> Executing PI CALC (%d iters)...\n", args->iterations);

      int inside_counts = task_compute_pi(args->iterations);

      res.value = inside_counts;
      res.count = args->iterations;
      break;
    }
    case TaskOp::FIND_PRIMES: {
      const ComputeArgs *args = (const ComputeArgs *)data_ptr;
      printf("   -> Executing PRIME SEARCH (limit %d)...\n", args->iterations);

      res.value = task_find_primes(args->iterations);
      break;
    }
    default:
      printf("   -> [Error] Unknown Task OpCode!\n");
      return; // Don't reply if unknown
    }

    Message reply;
    reply.sender_id = my_id;
    reply.type = MsgType::TASK_RESULT;
    std::memcpy(reply.payload, &res, sizeof(TaskResult));

    send_message(src_socket, &reply);
    printf("   -> [Job %d] Result sent back.\n", header->job_id);
    break;
  }

  case MsgType::TASK_RESULT: {
    TaskResult res;
    std::memcpy(&res, msg->payload, sizeof(TaskResult));

    if (res.count > 0) {
      double pi_est = 4.0 * ((double)res.value / (double)res.count);
      printf("[Job %d] COMPLETE! PI Estimate: %f (Points: %d/%d)\n", res.job_id,
             pi_est, res.value, res.count);
    } else {
      printf("[Job %d] COMPLETE! Result: %d\n", res.job_id, res.value);
    }
    break;
  }

  case MsgType::DATA:
    break;

  default:
    printf("[Node %d] Unknown message type\n", my_id);
    break;
  }
}

// ==========================================
// LOAD BALANCING LOGIC
// ==========================================

int find_best_node(int my_id) {
  int best_node_id = -1;
  int max_score = -10000;
  int current_id = 0;

  for (const NodeStatus &node : cluster_status) {
    if (current_id == 0 || current_id == my_id) {
      current_id++;
      continue;
    }

    if (node.ram_free_kb > 0) {
      int score = node.calculate_score();
      if (score > max_score) {
        max_score = score;
        best_node_id = current_id;
      }
    }
    current_id++;
  }
  return best_node_id;
}

void handle_disconnect(int node_id) {
  if (node_id > 0 && node_id <= MAX_PEERS) {
    std::memset(&cluster_status[node_id], 0, sizeof(NodeStatus));
    printf("[Logic] Cleared status for Node %d (Disconnected)\n", node_id);
  }
}

void mark_node_busy(int node_id) {
  if (node_id > 0 && node_id <= MAX_PEERS) {
    cluster_status[node_id].task_queue_len++;
  }

  printf("[Logic] Penalizing Node %d score (Queue: %d)\n", node_id,
         cluster_status[node_id].task_queue_len);
}
