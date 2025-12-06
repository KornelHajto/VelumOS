#include "../include/network.h"
#include "../include/protocol.h"
#include <cstdio>
#include <cstring>

using namespace velum;

NodeStatus cluster_status[MAX_PEERS + 1];

void handle_message(int my_id, Message *msg, int src_socket) {
  switch (msg->type) {
  case MsgType::STATUS_REPORT: {
    if (msg->sender_id <= MAX_PEERS) {
      std::memcpy(&cluster_status[msg->sender_id], msg->payload,
                  sizeof(NodeStatus));
      int score = cluster_status[msg->sender_id].calculate_score();
      printf("[Node %d] Updated Node %d status (Score: %d)\n", my_id,
             msg->sender_id, score);
    }
    break;
  }

  case MsgType::HEARTBEAT:
    printf("[Node %d] HEARTBEAT from Node %d\n", my_id, msg->sender_id);
    break;

  case MsgType::TASK_REQUEST: {
    TaskRequest task;
    std::memcpy(&task, msg->payload, sizeof(TaskRequest));
    printf("[Task] Received Request from Node %d: %d + %d\n", msg->sender_id,
           task.a, task.b);

    int sum = task.a + task.b;

    TaskResult result;
    result.result = sum;

    Message reply_msg;
    reply_msg.sender_id = my_id;
    reply_msg.type = MsgType::TASK_RESULT;
    std::memcpy(reply_msg.payload, &result, sizeof(TaskResult));

    send_message(src_socket, &reply_msg);
    printf("[Task] Sent Result %d back to Node %d\n", sum, msg->sender_id);
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
