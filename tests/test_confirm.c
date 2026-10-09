/* Tests for the HMAC key-confirmation helpers (Task 5).
 * export_app_key needs a live TLS session and is covered by the Task 6
 * integration test. */
#include "confirm.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); failures++; } } while (0)

int main(void) {
  printf("== test_confirm ==\n");

  unsigned char key[PQC_KEY_LEN];
  unsigned char id[PQC_ID_LEN];
  for (int i = 0; i < PQC_KEY_LEN; i++) key[i] = (unsigned char)(i + 1);
  for (int i = 0; i < PQC_ID_LEN; i++) id[i] = (unsigned char)(0xA0 + i);

  unsigned char t1[32], t2[32], ts[32];
  confirm_tag(key, "client-confirm", id, sizeof(id), t1);
  confirm_tag(key, "client-confirm", id, sizeof(id), t2);
  confirm_tag(key, "server-confirm", id, sizeof(id), ts);

  CHECK(memcmp(t1, t2, 32) == 0, "confirm_tag is deterministic");
  CHECK(memcmp(t1, ts, 32) != 0, "different label -> different tag");

  /* a tag is not all-zero (proves it actually computed something) */
  unsigned char zero[32] = {0};
  CHECK(memcmp(t1, zero, 32) != 0, "tag is non-trivial");

  CHECK(confirm_verify(key, "client-confirm", id, sizeof(id), t1) == 1,
        "verify accepts matching tag");

  unsigned char bad[32];
  memcpy(bad, t1, 32);
  bad[7] ^= 0x01;
  CHECK(confirm_verify(key, "client-confirm", id, sizeof(id), bad) == 0,
        "verify rejects a flipped-byte tag");

  CHECK(confirm_verify(key, "server-confirm", id, sizeof(id), t1) == 0,
        "verify rejects a wrong-label tag");

  printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
