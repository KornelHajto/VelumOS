#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../hal/hal_network.h"
#include "raft.h"


#define ELECTION_TIMEOUT 5

enum RaftState role;
int voted;
int term; //election

int vote_cnt;
time_t rnd_timeout;
time_t vote_start_time;
time_t last_heartbeat_time;

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
    
    vote_cnt = 0;
    vote_start_time = 0;
    rnd_timeout = 0;
    srand(time(NULL));

    role = FOLLOWER;
    voted = -1;
    term = 0;

    hal_init(my_id);
    last_heartbeat_time = 0;

    while (1) {
        RaftPacket rp = {
            .term = 0,
            .type = HEARTBEAT, //from the enum RaftPacketType
            .sender_id = my_id
        };

        hal_net_poll();

        switch(role) {
            case FOLLOWER:
                if(time(NULL) - last_heartbeat_time > ELECTION_TIMEOUT){
                    role = CANDIDATE;
                    term++;
                    voted = my_id;
                    last_heartbeat_time = time(NULL);
                    int target_id = 3 - my_id;
                    vote_start_time = time(NULL);
                    vote_cnt++;
                    rp.term = term;
                    rp.type = REQUEST_VOTE;
                    hal_net_send(target_id, (uint8_t*)&rp, sizeof(RaftPacket));
                }
                break;
            case CANDIDATE:
                if(rnd_timeout == 0){
                    rnd_timeout = (rand() % 7 ) + 6;
                    vote_start_time = time(NULL);
                    printf("[CANDIDATE] Starting Term %d. Timeout set to %ld seconds.\n", term, rnd_timeout);
                }

                if (time(NULL) - vote_start_time > rnd_timeout){
                    printf("[CANDIDATE] Election failed/timed out. Reverting to FOLLOWER.\n");
                    role = FOLLOWER;
                    rnd_timeout = 0;
                    break;
                }
                if (vote_cnt >= 2){
                    printf("[CANDIDATE] Received majority votes. Becoming LEADER.\n");
                    role = LEADER;
                    rnd_timeout = 0;
                    break;
                }
                sleep(1);
                break;
            case LEADER:
                rp.term = term;
                rp.type = HEARTBEAT;
                int target_id = 3 - my_id;
                hal_net_send(target_id, (uint8_t*)&rp, sizeof(RaftPacket));
                sleep(1);
                break;
            default:
                break;
        }

    }
    return 0;
}