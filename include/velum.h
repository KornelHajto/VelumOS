#ifndef VELUM_OS_H
#define VELUM_OS_H

#include <cstdint>
#include <string>
#include <vector>

#include "../common/protocol.h"

// Public API
void velum_init(int node_id, int port);
void velum_spawn(velum::TaskOp op, uint32_t work_amount);
void velum_spawn_wasm(const char *filepath, const char *func,
                      std::vector<std::string> args);
void velum_spawn_wasm_distributed(const char *filepath, const char *func,
                                  int start, int end);
#endif
