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
  uint32_t term;
  uint32_t candidate_id;
  uint32_t last_log_index;
  uint32_t last_log_term;
} VoteRequest;

typedef struct {
  uint32_t term;
  uint8_t vote_granted; // 1 if granted, 0 if denied
} VoteResponse;

typedef struct {
  uint32_t term;
  uint32_t leader_id;
  // Log entries would go here
} AppendEntries;

typedef struct {
  uint32_t sender_id;
  uint32_t term;
  uint8_t type;
  uint8_t payload[256];

} Message;

#pragma pack(pop)

#endif // !PROTOCOL_H
