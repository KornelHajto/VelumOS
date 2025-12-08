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
  TASK_RESULT,
  TASK_PROGRESS
};

enum class TaskOp : uint8_t { MATH_ADD, MATH_SUB, COMPUTE_PI, FIND_PRIMES };

struct TaskHeader {
  TaskOp op_code;
  uint32_t job_id;
};

struct MathArgs {
  int a;
  int b;
};

struct ComputeArgs {
  uint32_t iterations;
};

struct TaskResult {
  uint32_t job_id;
  int32_t value;
  uint32_t count;
};

struct NodeStatus {
  uint8_t cpu_load;
  uint16_t ram_free_kb;
  uint8_t task_queue_len;

  int calculate_score() const {
    const int WEIGHT_CPU = 2;
    const int WEIGHT_QUEUE = 10;
    int score = ram_free_kb;
    score -= (cpu_load * WEIGHT_CPU);
    score -= (task_queue_len * WEIGHT_QUEUE);
    return score;
  }
};

struct Message {
  uint16_t sender_id;
  MsgType type;
  uint8_t payload[4096]; // We needed to modify this because once WASM support
                         // is added 256 bytes would not have been enough.
};

} // namespace velum
#pragma pack(pop)

#endif // !PROTOCOL_H
