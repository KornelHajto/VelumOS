#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <unistd.h>

// Include Protocol for TaskOp
#include "../common/protocol.h"

// Forward declare the API functions
// (In a real library, these would be in velum.h)
void velum_init(int id, int port);
void velum_spawn(velum::TaskOp op, uint32_t work_amount);

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  if (argc != 2) {
    printf("Usage: %s <id>\n", argv[0]);
    return 1;
  }
  int my_id = atoi(argv[1]);

  // 1. Start the OS (Background)
  velum_init(my_id, 8000 + my_id);

  // 2. User Input Loop
  printf(">>> User App Ready. Type 'pi <amount>' to scatter work.\n");

  char buffer[256];
  while (true) {
    // Simple non-blocking read for demo purposes
    int nbytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    if (nbytes > 0) {
      buffer[nbytes] = 0;
      int val = 0;
      if (sscanf(buffer, "pi %d", &val) >= 1) {
        // CALL THE API
        velum_spawn(velum::TaskOp::COMPUTE_PI, val);
      }
    }
    usleep(10000); // Prevent CPU hogging in this simple loop
  }
  return 0;
}
