#include "../hal_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int sock_fd;


void hal_init(int my_node_id){
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0){
        perror("ERROR: Socket creation failed");
        exit(1);
    }

    int my_port = 8000 + my_node_id;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(my_port);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("ERROR: Bind failed");
        exit(1);
    }

    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

    printf("[HAL] Node %d Online on Port %d\n", my_node_id, my_port);
}


void hal_net_send(int target_id, uint8_t* data, size_t len){
    
    
    int my_port = 8000 + target_id;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(my_port);

    sendto(sock_fd, data, len, 0, (struct sockaddr *)&addr, sizeof(addr));

}

void hal_net_poll(void){
    char buffer[1024];
    struct sockaddr_in sender;
    socklen_t len = sizeof(sender);
    
    int n = recvfrom(sock_fd, buffer, 1024, 0, (struct sockaddr *)&sender, &len);
    
    if (n > 0) {
        buffer[n] = '\0';
        printf("[HAL] Received Packet: %s\n", buffer);
    }
}