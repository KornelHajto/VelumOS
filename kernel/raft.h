#ifndef RAFT_H
#define RAFT_H

#include <stdint.h>
#include <stddef.h>

enum RaftPacketType {
    REQUEST_VOTE,
    GRANT_VOTE,
    HEARTBEAT
};

enum RaftState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

typedef struct {
    uint32_t term; //election cycle 
    uint8_t type; //data
    uint8_t sender_id;
    uint8_t payload[]; //for extra data
} RaftPacket;


extern void handle_packet(RaftPacket*);

#endif