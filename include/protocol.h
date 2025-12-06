#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#pragma pack(push, 1)

namespace velum {

enum class MsgType : uint8_t { HEARTBEAT, STATUS_REPORT, DATA };

struct NodeStatus {
  uint8_t cpu_load;
  uint16_t ram_free_kb;
  uint8_t task_queue_len;
};

struct Message {
  uint16_t sender_id;
  MsgType type;
  uint8_t payload[256];
};

} // namespace velum
#pragma pack(pop)

#endif // !PROTOCOL_H
