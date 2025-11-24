#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "../hal/hal_network.h"
#include "raft.h"

enum RaftState role;
int voted;
int term; //election


int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "ERROR: Zero argument, 1 argument is required.\n");
        exit(1);
    }
    int my_id = atoi(argv[1]);
    if(my_id == 0){
        fprintf(stderr, "ERROR: Argument is not a number.\n");
        exit(1);
    }
    
    role = FOLLOWER;
    voted = -1;
    term = 0;

    hal_init(my_id);

    while (1) {
        RaftPacket rp = {
            .term = 0,
            .type = HEARTBEAT, //from the enum RaftPacketType
            .sender_id = my_id
        };

        hal_net_poll();

        switch(role) {
            case FOLLOWER:
                break;
            case CANDIDATE:
                break;
            case LEADER:
                rp.term = term;
                rp.type = HEARTBEAT;
                int target_id = 3 - my_id;
                hal_net_send(target_id, (uint8_t*)&rp, sizeof(RaftPacket));
                break;
            default:
                break;
        }

    }
    return 0;
}