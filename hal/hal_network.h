#ifndef HAL_NETWORK_H
#define HAL_NETWORK_H

#include <stdint.h>
#include <stddef.h>


/* Initialization */
void hal_init(int my_node_id);

/* Sending packet to specified neighbor */
void hal_net_send(int target_id, uint8_t* data, size_t len);

/* Prevents freezing */
void hal_net_poll(void);

#endif