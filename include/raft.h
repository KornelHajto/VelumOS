#ifndef RAFT_H
#define RAFT_H

#include "protocol.h"
#include <stdint.h>

// Raft Constants
#define ELECTION_TIMEOUT_MIN_MS 1500
#define ELECTION_TIMEOUT_MAX_MS 3000
#define HEARTBEAT_INTERVAL_MS 1000

typedef enum { RAFT_FOLLOWER, RAFT_CANDIDATE, RAFT_LEADER } RaftRole;

typedef struct {
  uint32_t id;
  uint32_t num_nodes;
  RaftRole state;
  uint32_t current_term;
  int32_t voted_for; // -1 if none

  // Volatile state
  uint32_t leader_id; // Current leader ID
  uint64_t last_heartbeat_time;
  uint64_t next_election_time;

  // Candidate state
  int votes_received;

} RaftNode;

// Public API
void raft_init(RaftNode *raft, uint32_t id, uint32_t num_nodes);
void raft_tick(RaftNode *raft,
               int (*send_msg_func)(int target_id, Message *msg));
void raft_handle_msg(
    RaftNode *raft, Message *msg,
    int (*send_msg_func)(int target_id,
                         Message *msg)); // Pass a callback or use global
                                         // network? Callback is cleaner.

// Helper to get current time in ms
uint64_t get_current_time_ms();

#endif // RAFT_H
