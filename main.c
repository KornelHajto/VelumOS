#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define BASE_PORT 8000

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <Node ID (1-3)>\n", argv[0]);
    return 1;
  }

  int my_id = atoi(argv[1]);
  int my_port = BASE_PORT + my_id;
  int client_sockets[MAX_CLIENTS];
  int max_sd, sd, activity, new_socket;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  char buffer[BUFFER_SIZE];

  // Initialize client sockets array to 0
  for (int i = 0; i < MAX_CLIENTS; i++) {
    client_sockets[i] = 0;
  }

  // --- 1. Setup Server Socket ---
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == 0) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  // Allow multiple sockets to use the same port (useful for restarting quickly)
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(my_port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  printf("Node %d listening on port %d...\n", my_id, my_port);

  // --- 2. Try to Connect to Peers (Mesh) ---
  // We try to connect to nodes 1, 2, and 3 (skipping ourselves)
  for (int i = 1; i <= 3; i++) {
    if (i == my_id)
      continue; // Don't connect to self

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(BASE_PORT + i);

    // Convert IPv4 and IPv6 addresses from text to binary form
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      printf("Could not connect to Node %d (maybe not up yet?)\n", i);
      close(sock);
    } else {
      printf("Connected to Node %d!\n", i);
      // Add this new connection to our list
      for (int j = 0; j < MAX_CLIENTS; j++) {
        if (client_sockets[j] == 0) {
          client_sockets[j] = sock;
          break;
        }
      }
    }
  }

  fd_set readfds;

  // --- 3. The Event Loop ---
  while (1) {
    FD_ZERO(&readfds);

    // Add server socket (listener)
    FD_SET(server_fd, &readfds);
    max_sd = server_fd;

    // Add STDIN (so you can type messages)
    FD_SET(STDIN_FILENO, &readfds);
    if (STDIN_FILENO > max_sd)
      max_sd = STDIN_FILENO;

    // Add all connected neighbors
    for (int i = 0; i < MAX_CLIENTS; i++) {
      sd = client_sockets[i];
      if (sd > 0)
        FD_SET(sd, &readfds);
      if (sd > max_sd)
        max_sd = sd;
    }

    // Wait for activity
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR)) {
      printf("select error");
    }

    // A. If something happened on the Listener (New Connection)
    if (FD_ISSET(server_fd, &readfds)) {
      if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                               (socklen_t *)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
      }
      printf("New peer connected: socket fd is %d\n", new_socket);

      // Add to array
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == 0) {
          client_sockets[i] = new_socket;
          break;
        }
      }
    }

    // B. If YOU typed something (STDIN)
    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      int nread = read(STDIN_FILENO, buffer, BUFFER_SIZE);
      if (nread > 0) {
        buffer[nread] = '\0'; // Null terminate
        // Broadcast to all connected peers
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (client_sockets[i] != 0) {
            send(client_sockets[i], buffer, strlen(buffer), 0);
          }
        }
      }
    }

    // C. If a Neighbor sent data
    for (int i = 0; i < MAX_CLIENTS; i++) {
      sd = client_sockets[i];
      if (FD_ISSET(sd, &readfds)) {
        int valread = read(sd, buffer, BUFFER_SIZE);
        if (valread == 0) {
          // Somebody disconnected
          printf("Node disconnected.\n");
          close(sd);
          client_sockets[i] = 0;
        } else {
          buffer[valread] = '\0';
          printf("Received: %s", buffer);
        }
      }
    }
  }
  return 0;
}
