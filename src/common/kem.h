#ifndef PQC_KEM_H
#define PQC_KEM_H
#include <stdint.h>
/* ML-KEM-768 micro-operations via the OpenSSL EVP API.
 * All functions return 0 on success, non-zero on a hard error.
 * Timings are of the crypto operation only; buffer allocation and library
 * init are excluded from the op timers. */

#define KEM_ALG "ML-KEM-768"

/* Force provider load / first-use cost; report it in *init_ns. */
int kem_init_once(uint64_t *init_ns);

/* One full cycle with a FRESH keypair. Times keygen/encaps/decaps separately
 * (nanoseconds, CLOCK_MONOTONIC) and reports whether the two shared secrets
 * match (*match = 1/0). */
int kem_roundtrip(uint64_t *keygen_ns, uint64_t *encaps_ns,
                  uint64_t *decaps_ns, int *match);

/* Implicit-rejection check: encapsulate, flip one byte of a correct-length
 * ciphertext, decapsulate, and report whether the secret DIFFERS from the
 * original (*differs = 1 means it differs — the expected, correct outcome).
 * Does NOT require the decapsulation to return an API error. */
int kem_implicit_rejection_differs(int *differs);

#endif /* PQC_KEM_H */
