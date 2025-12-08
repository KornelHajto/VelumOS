#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "../include/velum.h"

void velum_spawn_wasm_distributed(const char *filepath, const char *func,
                                  int start, int end);

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  if (argc != 2) {
    printf("Usage: %s <id>\n", argv[0]);
    return 1;
  }
  int my_id = atoi(argv[1]);

  velum_init(my_id, 8000 + my_id);

  printf(">>> User App Ready.\n");
  printf("    Type: wasm <file> <func> <arg1> ... (Single execution)\n");
  printf("    Type: scatter <file> <func> <start> <end> (Distributed "
         "execution)\n");
  printf("    Type: pi <iterations> (Legacy benchmark)\n");

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty())
      continue;

    std::stringstream ss(line);
    std::string cmd;
    ss >> cmd;

    if (cmd == "wasm") {
      std::string file, func;
      ss >> file >> func;

      std::vector<std::string> args;
      std::string temp;
      while (ss >> temp)
        args.push_back(temp);

      if (!file.empty() && !func.empty()) {
        velum_spawn_wasm(file.c_str(), func.c_str(), args);
      }
    } else if (cmd == "scatter") {
      std::string file, func;
      int start, end;
      if (ss >> file >> func >> start >> end) {
        velum_spawn_wasm_distributed(file.c_str(), func.c_str(), start, end);
      } else {
        printf("Error: Usage is 'scatter <file> <func> <start> <end>'\n");
      }
    } else if (cmd == "pi") {
      uint32_t val;
      if (ss >> val) {
        velum_spawn(velum::TaskOp::COMPUTE_PI, val);
      }
    }
  }
  return 0;
}
