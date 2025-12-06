#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#pragma pack(push, 1)

namespace velum {

enum class MsgType : uint8_t {
  HEARTBEAT,
  STATUS_REPORT,
  DATA,
  TASK_REQUEST,
  TASK_RESULT
};

enum class TaskOp : uint8_t { ADD, SUBTRACT, MULTIPLY, DIVIDE };

struct TaskHeader {
  TaskOp op_code;
};

struct MathArgs {
  int a;
  int b;
};

struct TaskResult {
  int result;
};

struct NodeStatus {
  uint8_t cpu_load;
  uint16_t ram_free_kb;
  uint8_t task_queue_len;

  int calculate_score() const {
    const int WEIGHT_CPU = 2;
    const int WEIGHT_QUEUE = 10;
    int score = ram_free_kb;

    // basically first param must be ram becasuse on esp32 if out of ram, then
    // the board reboots/crashes.
    score -= (cpu_load * WEIGHT_CPU);
    score -= (task_queue_len * WEIGHT_QUEUE);
    return score;
  }
};

struct Message {
  uint16_t sender_id;
  MsgType type;
  uint8_t payload[256];
};

} // namespace velum
#pragma pack(pop)

#endif // !PROTOCOL_H
