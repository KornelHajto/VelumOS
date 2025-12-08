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
// [!!!] ADDED: Forward declaration for the Wasm spawner
void velum_spawn_wasm(const char *filepath, const char *func, int param);

int main(int argc, char *argv[]) {
  // Disable buffering so logs appear instantly
  setbuf(stdout, NULL);

  if (argc != 2) {
    printf("Usage: %s <id>\n", argv[0]);
    return 1;
  }
  int my_id = atoi(argv[1]);

  // 1. Start the OS (Background)
  velum_init(my_id, 8000 + my_id);

  // 2. User Input Loop
  printf(">>> User App Ready.\n");
  printf("    Type 'pi <amount>' to scatter work.\n");
  printf("    Type 'wasm <file> <func> <arg>' to run dynamic code.\n");

  char buffer[256];
  while (true) {
    int nbytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    if (nbytes > 0) {
      buffer[nbytes] = 0;

      // Variables for parsing
      int val = 0;
      char file[64];
      char func[32];
      int param = 0;

      // [!!!] ADDED: Check for "wasm" command first
      if (sscanf(buffer, "wasm %s %s %d", file, func, &param) >= 3) {
        velum_spawn_wasm(file, func, param);
      }
      // Check for "pi" command
      else if (sscanf(buffer, "pi %d", &val) >= 1) {
        velum_spawn(velum::TaskOp::COMPUTE_PI, val);
      }
    }
    usleep(10000);
  }
  return 0;
}
