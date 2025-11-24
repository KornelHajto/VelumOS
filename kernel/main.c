#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "../hal/hal_network.h"

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "ERROR: Zero argument, 1 argument is required.");
        exit(1);
    }
    int my_id = atoi(argv[1]);
    if(my_id == 0){
        fprintf(stderr, "ERROR: Argument is not a number.");
        exit(1);
    }
    
    hal_init(my_id);

    while (1) {
        hal_net_poll();

        if (my_id == 1) {
            char* msg = "PING from Node 1";
            hal_net_send(2, (uint8_t*)msg, strlen(msg));
            sleep(1);
        }

        if(my_id == 2){
            char* msg = "PONG from Node 2";
            hal_net_send(1, (uint8_t*)msg, strlen(msg));
            sleep(1);
        }
    }
    return 0;
}