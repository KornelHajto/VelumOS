#include "../include/user_tasks.h"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

int task_compute_pi(uint32_t iterations) {
  int inside = 0;
  for (uint32_t i = 0; i < iterations; i++) {
    double x = (double)rand() / RAND_MAX;
    double y = (double)rand() / RAND_MAX;
    if (x * x + y * y <= 1.0)
      inside++;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  return inside;
}

int task_find_primes(uint32_t limit) {
  int count = 0;
  for (uint32_t num = 2; num < limit; num++) {
    bool prime = true;
    for (uint32_t div = 2; div * div <= num; div++) {
      if (num % div == 0) {
        prime = false;
        break;
      }
    }
    if (prime) {
      count++;
    }
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  return count;
}
