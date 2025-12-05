#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#pragma pack(push, 1)

typedef enum {
  MSG_HEARTBEAT,
  MSG_VOTE_REQUEST,
  MSG_VOTE_ACK,
  MSG_DATA
} MsgType;

typedef struct {
  uint32_t sender_id;
  uint32_t term;
  uint8_t type;
  uint8_t payload[256];

} Message;

#pragma pack(pop)

#endif // !PROTOCOL_H
