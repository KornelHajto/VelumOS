// tests/primes.c
// compile: clang --target=wasm32 -O3 -nostdlib -Wl,--no-entry -Wl,--export-all
// -o tests/primes.wasm tests/primes.c

// Simple check helper (internal)
int is_prime(int n) {
  if (n <= 1)
    return 0;
  if (n <= 3)
    return 1;
  if (n % 2 == 0 || n % 3 == 0)
    return 0;

  for (int i = 5; i * i <= n; i = i + 6) {
    if (n % i == 0 || n % (i + 2) == 0)
      return 0;
  }
  return 1;
}

// The Distributed Task
// Input: Range [start, end)
// Output: Count of primes found in that chunk
int count_primes(int start, int end) {
  int count = 0;
  for (int i = start; i < end; i++) {
    if (is_prime(i)) {
      count++;
    }
  }
  return count;
}
