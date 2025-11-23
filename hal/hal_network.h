#ifndef HAL_NETWORK_H
#define HAL_NETWORK_H

#include <stdint.h>
#include <stddef.h>

enum RaftPacketType {
    REQUEST_VOTE,
    GRANT_VOTE,
    HEARTBEAT
};


typedef struct {
    uint32_t term; //election cycle 
    uint8_t type; //data
    uint8_t sender_id;
    uint8_t payload[]; //for extra data
} RaftPacket;


/* Initialization */
void hal_init(int my_node_id);

/* Sending packet to specified neighbor */
void hal_net_send(int target_id, uint8_t* data, size_t len);

/* Prevents freezing */
void hal_net_poll(void);

#endif