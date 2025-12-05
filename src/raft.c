#include "../include/raft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// Get ms time
uint64_t get_current_time_ms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// Random timeout between MIN and MAX
static uint32_t get_random_timeout() {
  return ELECTION_TIMEOUT_MIN_MS +
         (rand() % (ELECTION_TIMEOUT_MAX_MS - ELECTION_TIMEOUT_MIN_MS));
}

void raft_init(RaftNode *raft, uint32_t id, uint32_t num_nodes) {
  if (!raft)
    return;
  raft->id = id;
  raft->num_nodes = num_nodes;
  raft->state = RAFT_FOLLOWER;
  raft->current_term = 0;
  raft->voted_for = -1;
  raft->votes_received = 0;
  raft->leader_id = 0;
  raft->last_heartbeat_time = get_current_time_ms();
  raft->next_election_time = get_current_time_ms() + get_random_timeout();
  srand(time(NULL) + id); // Seed RNG
}

static void send_request_vote(RaftNode *raft,
                              int (*send_msg_func)(int, Message *)) {
  Message msg;
  msg.sender_id = raft->id;
  msg.term = raft->current_term;
  msg.type = MSG_VOTE_REQUEST;

  VoteRequest *body = (VoteRequest *)msg.payload;
  body->term = raft->current_term;
  body->candidate_id = raft->id;
  body->last_log_index = 0; // TODO: Log implementation
  body->last_log_term = 0;

  printf("[Node %d] Starting ELECTION. Term %d\n", raft->id,
         raft->current_term);

  // Broadcast to peers
  for (int i = 1; i <= raft->num_nodes; i++) {
    if (i != raft->id) {
      send_msg_func(i, &msg);
    }
  }
}

static void send_heartbeat(RaftNode *raft,
                           int (*send_msg_func)(int, Message *)) {
  Message msg;
  msg.sender_id = raft->id;
  msg.term = raft->current_term;
  msg.type = MSG_HEARTBEAT;

  AppendEntries *body = (AppendEntries *)msg.payload;
  body->term = raft->current_term;
  body->leader_id = raft->id;

  // printf("[Node %d] Sending Heartbeat (Term %d)\n", raft->id,
  // raft->current_term);

  for (int i = 1; i <= raft->num_nodes; i++) {
    if (i != raft->id) {
      send_msg_func(i, &msg);
    }
  }
}

void raft_tick(RaftNode *raft, int (*send_msg_func)(int, Message *)) {
  uint64_t now = get_current_time_ms();

  if (raft->state == RAFT_LEADER) {
    if (now - raft->last_heartbeat_time >= HEARTBEAT_INTERVAL_MS) {
      send_heartbeat(raft, send_msg_func);
      raft->last_heartbeat_time = now;
    }
  } else {
    // Follower or Candidate
    if (now >= raft->next_election_time) {
      // Election Timeout! Become Candidate
      raft->state = RAFT_CANDIDATE;
      raft->current_term++;
      raft->voted_for = raft->id;
      raft->votes_received = 1; // Vote for self
      raft->next_election_time = now + get_random_timeout();

      send_request_vote(raft, send_msg_func);
    }
  }
}

void raft_handle_msg(RaftNode *raft, Message *msg,
                     int (*send_msg_func)(int, Message *)) {
  // 1. Term check: If RPC request or response contains term T > currentTerm:
  // set currentTerm = T, convert to follower
  if (msg->term > raft->current_term) {
    printf("[Node %d] Seen higher term %d (current %d). Stepping down.\n",
           raft->id, msg->term, raft->current_term);
    raft->current_term = msg->term;
    raft->state = RAFT_FOLLOWER;
    raft->voted_for = -1;
    raft->votes_received = 0;
    // Reset timer
    raft->next_election_time = get_current_time_ms() + get_random_timeout();
  }

  switch (msg->type) {
  case MSG_VOTE_REQUEST: {
    VoteRequest *req = (VoteRequest *)msg->payload;
    int granted = 0;
    // Simple voting logic: Vote if term valid and not voted yet
    if (req->term >= raft->current_term &&
        (raft->voted_for == -1 || raft->voted_for == req->candidate_id)) {
      raft->voted_for = req->candidate_id;
      granted = 1;
      // Reset election timer since we granted a vote
      raft->next_election_time = get_current_time_ms() + get_random_timeout();
    }

    Message response;
    response.sender_id = raft->id;
    response.term = raft->current_term;
    response.type = MSG_VOTE_ACK;
    VoteResponse *res = (VoteResponse *)response.payload;
    res->term = raft->current_term;
    res->vote_granted = granted;

    send_msg_func(msg->sender_id, &response);
    if (granted) {
      printf("[Node %d] Voted FOR Node %d (Term %d)\n", raft->id,
             req->candidate_id, raft->current_term);
    }
    break;
  }

  case MSG_VOTE_ACK: {
    if (raft->state == RAFT_CANDIDATE) {
      VoteResponse *res = (VoteResponse *)msg->payload;
      if (res->vote_granted && res->term == raft->current_term) {
        raft->votes_received++;
        printf("[Node %d] Received vote from %d. Total: %d\n", raft->id,
               msg->sender_id, raft->votes_received);

        // Helper: Dynamic majority calculation
        int majority = (raft->num_nodes / 2) + 1;
        if (raft->votes_received >= majority) {
          printf("!!! [Node %d] BECAME LEADER (Term %d) !!!\n", raft->id,
                 raft->current_term);
          raft->state = RAFT_LEADER;
          raft->leader_id = raft->id;
          send_heartbeat(raft, send_msg_func);
          raft->last_heartbeat_time = get_current_time_ms();
        }
      }
    }
    break;
  }

  case MSG_HEARTBEAT: {
    AppendEntries *ae = (AppendEntries *)msg->payload;
    if (ae->term >= raft->current_term) {
      raft->state = RAFT_FOLLOWER;
      raft->leader_id = ae->leader_id;
      raft->next_election_time = get_current_time_ms() + get_random_timeout();
      // printf("[Node %d] Heartbeat from Leader %d\n", raft->id,
      // ae->leader_id);
    }
    break;
  }
  }
}
