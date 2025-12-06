#include "../include/network.h"
#include "../include/protocol.h"
#include <cstdio>
#include <cstring>

using namespace velum;

NodeStatus cluster_status[MAX_PEERS + 1];

int process_task(const uint8_t *payload) {
  const TaskHeader *header = (const TaskHeader *)payload;
  const MathArgs *args = (const MathArgs *)(payload + sizeof(TaskHeader));

  int result = 0;
  switch (header->op_code) {
  case TaskOp::ADD:
    printf("[Task] Executing ADD: %d + %d\n", args->a, args->b);
    result = args->a + args->b;
    break;
  case TaskOp::SUBTRACT:
    printf("[Task] Executing SUB: %d - %d\n", args->a, args->b);
    result = args->a - args->b;
    break;
  case TaskOp::MULTIPLY:
    printf("[Task] Executing MUL: %d * %d\n", args->a, args->b);
    result = args->a * args->b;
    break;
  case TaskOp::DIVIDE:
    printf("[Task] Executing MUL: %d * %d\n", args->a, args->b);
    result = args->a / args->b;
    break;
  default:
    break;
  }
  return result;
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
    printf("[Task] Received Task Request from Node %d\n", msg->sender_id);

    int math_result = process_task(msg->payload);

    TaskResult res_struct;
    res_struct.result = math_result;

    Message reply;
    reply.sender_id = my_id;
    reply.type = MsgType::TASK_RESULT;
    std::memcpy(reply.payload, &res_struct, sizeof(TaskResult));

    send_message(src_socket, &reply);
    printf("[Task] Sent Result %d back to Node %d\n", math_result,
           msg->sender_id);
    break;
  }

  case MsgType::TASK_RESULT: {
    TaskResult result;
    std::memcpy(&result, msg->payload, sizeof(TaskResult));
    printf("[Task] JOB COMPLETE! Node %d returned result: %d\n", msg->sender_id,
           result.result);
    break;
  }

  case MsgType::DATA:
    printf("[Node %d] DATA received from Node %d\n", my_id, msg->sender_id);
    break;

  default:
    printf("[Node %d] Unknown message type\n", my_id);
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
    printf("[Logic] Cleared status for Node %d (Disconnected)\n", node_id);
  }
}
