#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "../include/velum.h"

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  if (argc != 2) {
    printf("Usage: %s <id>\n", argv[0]);
    return 1;
  }
  int my_id = atoi(argv[1]);

  velum_init(my_id, 8000 + my_id);

  printf(">>> Ready. Type: wasm <file> <func> <arg1> <arg2> ...\n");

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

      // Capture all remaining args
      std::vector<std::string> args;
      std::string temp;
      while (ss >> temp)
        args.push_back(temp);

      if (!file.empty() && !func.empty()) {
        velum_spawn_wasm(file.c_str(), func.c_str(), args);
      }
    }
  }
  return 0;
}
