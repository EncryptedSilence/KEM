/* Tests for the ML-KEM-768 primitive (Task 4). */
#include "kem.h"
#include <stdio.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); failures++; } } while (0)

int main(void) {
  printf("== test_kem ==\n");

  uint64_t init_ns = 0;
  CHECK(kem_init_once(&init_ns) == 0, "kem_init_once succeeds");

  /* A few honest cycles: every secret must match. */
  int all_match = 1;
  int any_timed = 1;
  for (int i = 0; i < 3; i++) {
    uint64_t kg = 0, en = 0, de = 0;
    int match = 0;
    if (kem_roundtrip(&kg, &en, &de, &match) != 0) { all_match = 0; break; }
    if (!match) all_match = 0;
    if (kg == 0 || en == 0 || de == 0) any_timed = 0;
  }
  CHECK(all_match, "3 cycles: shared secrets match");
  CHECK(any_timed, "keygen/encaps/decaps each timed (nonzero)");

  /* Implicit rejection: a corrupted ciphertext yields a different secret. */
  int differs = 0;
  CHECK(kem_implicit_rejection_differs(&differs) == 0, "implicit-rejection runs");
  CHECK(differs == 1, "corrupted ciphertext -> different secret");

  printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
