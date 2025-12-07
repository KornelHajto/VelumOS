#include "../common/protocol.h"
#include "../include/user_tasks.h"
#include <gtest/gtest.h>

// --- 1. MOCK DEPENDENCIES ---
namespace velum {
void send_message(int socket_fd, Message *msg) {}
} // namespace velum

// --- 2. EXTERNAL SYMBOLS ---
extern velum::NodeStatus cluster_status[];
extern int find_best_node(int my_id);

// --- TEST CASE 1: Math Logic ---
TEST(UserTasksTest, PrimeFinding) {
  EXPECT_EQ(task_find_primes(10), 4);

  EXPECT_EQ(task_find_primes(20), 8);
}

// --- TEST CASE 2: Load Balancer Logic ---
TEST(NodeLogicTest, FindBestNode) {
  using namespace velum;

  for (int i = 0; i <= 16; i++) {
    cluster_status[i] = {0, 0, 0};
  }

  cluster_status[2] = {10, 200, 0}; // 10% CPU, 200MB RAM, 0 Queue -> High Score
  cluster_status[3] = {90, 200, 5}; // 90% CPU, 200MB RAM, 5 Queue -> Low Score

  int winner = find_best_node(1);

  EXPECT_EQ(winner, 2);
}

// --- TEST CASE 3: Protocol Packing ---
TEST(ProtocolTest, StructAlignment) { EXPECT_EQ(sizeof(velum::TaskHeader), 5); }

// --- TEST CASE 4: Pi Logic (Range Check) ---
TEST(UserTasksTest, PiApproximation) {
  int total = 10000;
  int inside = task_compute_pi(total);

  EXPECT_GT(inside, 0);     // Should find points inside
  EXPECT_LT(inside, total); // Should find points outside (corners)
}

// --- TEST CASE 5: Load Balancer Saturation ---
TEST(NodeLogicTest, NoWorkersAvailable) {
  using namespace velum;

  for (int i = 0; i <= 16; i++) {
    cluster_status[i] = {100, 0, 50}; // 100% CPU, 0 RAM
  }

  // we crash all of the nodes
  int winner = find_best_node(1);

  // Assert: Should return -1 (No valid worker)
  EXPECT_EQ(winner, -1);
}

// --- TEST CASE 6: Input Boundaries ---
TEST(UserTasksTest, ZeroInput) {
  EXPECT_EQ(task_find_primes(0), 0); // Should handle 0 gracefully
  EXPECT_EQ(task_compute_pi(0), 0);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
