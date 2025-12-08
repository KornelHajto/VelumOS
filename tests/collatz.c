// tests/collatz.c

// The Collatz Conjecture:
// If n is even -> n / 2
// If n is odd  -> 3n + 1
// Repeat until n == 1.
// Returns the number of steps taken.

int run(int n) {
  int steps = 0;

  // Safety: Protect against 0 or negative inputs
  if (n <= 0)
    return 0;

  while (n > 1) {
    if (n % 2 == 0) {
      n = n / 2;
    } else {
      n = (n * 3) + 1;
    }
    steps++;

    // Safety: Break if it takes too long (simple cycle protection)
    if (steps > 10000)
      break;
  }

  return steps;
}
