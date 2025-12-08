// tests/pi_scatter.c
// compile: clang --target=wasm32 -O3 -nostdlib -Wl,--no-entry -Wl,--export-all
// -o tests/pi_scatter.wasm tests/pi_scatter.c

// A simple Linear Congruential Generator (LCG) for randomness
// Since we don't have <stdlib.h>, we build our own state.
unsigned long next_rand(unsigned long *state) {
  *state = (*state * 1103515245 + 12345) & 0x7fffffff;
  return *state;
}

// The Distributed Task
// range [start, end] is treated as "number of iterations to run"
// Returns: Number of points inside the circle
int pi_hits(int start, int end) {
  int hits = 0;
  int iterations = end - start;

  // Seed randomness based on the 'start' value
  // This ensures every node gets a different random sequence!
  unsigned long rng_state = (unsigned long)start;

  // Circle Radius (Integer math)
  const int R = 10000;
  const int R2 = R * R;

  for (int i = 0; i < iterations; i++) {
    // Generate random X, Y between 0 and 10000
    int x = next_rand(&rng_state) % (R + 1);
    int y = next_rand(&rng_state) % (R + 1);

    if ((x * x) + (y * y) <= R2) {
      hits++;
    }
  }
  return hits;
}
