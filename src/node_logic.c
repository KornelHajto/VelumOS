#include "../include/protocol.h"
#include <stdio.h>

void handle_message(int my_id, Message *msg) {
  switch (msg->type) {
  case MSG_HEARTBEAT:
    printf("[Node %d] Received HEARTBEAT from Node %d (Term %d)\n", my_id,
           msg->sender_id, msg->term);
    break;

  case MSG_VOTE_REQUEST:
    printf("[Node %d] Received VOTE REQUEST from Node %d for Term %d\n", my_id,
           msg->sender_id, msg->term);
    // TODO: Add logic to decide whether to vote or not
    break;

  case MSG_VOTE_ACK:
    printf("[Node %d] Received VOTE from Node %d\n", my_id, msg->sender_id);
    break;

  default:
    printf("[Node %d] Unknown message type from %d\n", my_id, msg->sender_id);
  }
};
