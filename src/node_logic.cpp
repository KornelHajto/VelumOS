#include "../common/protocol.h"
#include "../include/network.h"
#include "../include/user_tasks.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

using namespace velum;

NodeStatus cluster_status[MAX_PEERS + 1];

void mark_node_busy(int node_id) {
  if (node_id > 0 && node_id <= MAX_PEERS) {
    cluster_status[node_id].task_queue_len++;
  }
}

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

    printf("[Job %d] Received Task (Op: %d). Processing...\n", header->job_id,
           (int)header->op_code);

    TaskResult res;
    res.job_id = header->job_id;
    res.value = 0;
    res.count = 0;

    uint32_t total_work = 0;
    uint32_t chunk_size = 50000000;

    if (header->op_code == TaskOp::COMPUTE_PI ||
        header->op_code == TaskOp::FIND_PRIMES) {
      const ComputeArgs *args = (const ComputeArgs *)data_ptr;
      total_work = args->iterations;
    }

    if (total_work > chunk_size) {
      uint32_t remain = total_work;

      while (remain > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        uint32_t current_batch = (remain > chunk_size) ? chunk_size : remain;

        int chunk_result = 0;
        if (header->op_code == TaskOp::COMPUTE_PI)
          chunk_result = task_compute_pi(current_batch);
        else if (header->op_code == TaskOp::FIND_PRIMES)
          chunk_result = task_find_primes(current_batch);

        TaskResult progress;
        progress.job_id = header->job_id;
        progress.value = chunk_result;
        progress.count = current_batch;

        remain -= current_batch;

        Message update;
        update.sender_id = my_id;
        update.type =
            (remain == 0) ? MsgType::TASK_RESULT : MsgType::TASK_PROGRESS;
        std::memcpy(update.payload, &progress, sizeof(TaskResult));

        send_message(src_socket, &update);

        if (remain > 0) {
          printf("   -> [Job %d] Checkpoint sent (%d done, %d left)\n",
                 header->job_id, current_batch, remain);
        }
      }
      printf("   -> [Job %d] Final result sent.\n", header->job_id);

    } else {
      switch (header->op_code) {
      case TaskOp::MATH_ADD: {
        const MathArgs *args = (const MathArgs *)data_ptr;
        res.value = args->a + args->b;
        break;
      }
      case TaskOp::MATH_SUB: {
        const MathArgs *args = (const MathArgs *)data_ptr;
        res.value = args->a - args->b;
        break;
      }
      case TaskOp::COMPUTE_PI: {
        const ComputeArgs *args = (const ComputeArgs *)data_ptr;
        res.value = task_compute_pi(args->iterations);
        res.count = args->iterations;
        break;
      }
      case TaskOp::FIND_PRIMES: {
        const ComputeArgs *args = (const ComputeArgs *)data_ptr;
        res.value = task_find_primes(args->iterations);
        break;
      }
      default:
        break;
      }

      Message reply;
      reply.sender_id = my_id;
      reply.type = MsgType::TASK_RESULT;
      std::memcpy(reply.payload, &res, sizeof(TaskResult));
      send_message(src_socket, &reply);
      printf("   -> [Job %d] Result sent back.\n", header->job_id);
    }
    break;
  }

  case MsgType::TASK_RESULT:
    break;
  case MsgType::TASK_PROGRESS:
    break;
  default:
    break;
  }
}

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
  }
}
