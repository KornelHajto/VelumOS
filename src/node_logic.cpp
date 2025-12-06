#include "../include/protocol.h"
#include <cstdio>  // For printf
#include <cstring> // For memcpy

using namespace velum;

void handle_message(int my_id, Message *msg) {
  switch (msg->type) {
  case MsgType::STATUS_REPORT: {
    NodeStatus status;

    std::memcpy(&status, msg->payload, sizeof(NodeStatus));

    printf(
        "[Node %d] REPORT from Node %d: CPU: %d%% | RAM: %d KB | Tasks: %d\n",
        my_id, msg->sender_id, status.cpu_load, status.ram_free_kb,
        status.task_queue_len);
    break;
  }

  case MsgType::HEARTBEAT:
    printf("[Node %d] HEARTBEAT from Node %d\n", my_id, msg->sender_id);
    break;

  case MsgType::DATA:
    printf("[Node %d] DATA received from Node %d\n", my_id, msg->sender_id);
    break;

  default:
    printf("[Node %d] Unknown message type\n", my_id);
    break;
  }
}
