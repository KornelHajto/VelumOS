// tests/search.c
// Find a number 'x' in [start, end) where (x * 123) % 99999 == 1
// Returns the number if found, or 0.

int search(int start, int end) {
  for (int i = start; i < end; i++) {
    // Heavy mock calculation
    if ((i * 123) % 99999 == 1) {
      return i; // Found it!
    }
  }
  return 0; // Not found in this chunk
}
