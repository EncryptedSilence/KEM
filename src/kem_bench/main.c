/* kem_bench — local ML-KEM-768 micro-benchmark (TOR 4 + 8-local).
 *
 * Runs <warmup> warm-up + <count> measured cycles, a FRESH keypair each cycle,
 * timing keygen/encapsulation/decapsulation separately on CLOCK_MONOTONIC.
 * Library init and buffer allocation are excluded from the op timers.
 * Verifies the two shared secrets match every cycle, and runs the
 * implicit-rejection (corrupted-ciphertext) check once at the end. */
#include "kem.h"
#include "timing.h"
#include "csv.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *p) {
  fprintf(stderr,
    "usage: %s [--warmup N] [--count N] [--csv PATH] [--label TEXT]\n", p);
}

int main(int argc, char **argv) {
  long warmup = 100, count = 1000;
  const char *csv_path = NULL, *label = "kem";

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--warmup") && i + 1 < argc) warmup = atol(argv[++i]);
    else if (!strcmp(argv[i], "--count") && i + 1 < argc) count = atol(argv[++i]);
    else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else if (!strcmp(argv[i], "--label") && i + 1 < argc) label = argv[++i];
    else { usage(argv[0]); return 2; }
  }

  assert_openssl_version("3.5.9");
  printf("kem_bench: %s  warmup=%ld count=%ld\n", openssl_runtime_version(), warmup, count);
  printf("label=%s algorithm=%s\n", label, KEM_ALG);

  uint64_t init_ns = 0;
  if (kem_init_once(&init_ns) != 0) { fprintf(stderr, "kem_init_once failed\n"); return 1; }
  printf("init_ns=%llu (library init / first-use, excluded from op timers)\n",
         (unsigned long long)init_ns);

  Csv *csv = NULL;
  if (csv_path) {
    csv = csv_open(csv_path, "cycle,keygen_ns,encaps_ns,decaps_ns,secret_match");
    if (!csv) { fprintf(stderr, "cannot open csv %s\n", csv_path); return 1; }
  }

  /* Warm-up (not recorded). */
  for (long i = 0; i < warmup; i++) {
    uint64_t a, b, c; int m;
    if (kem_roundtrip(&a, &b, &c, &m) != 0 || !m) {
      fprintf(stderr, "warmup cycle %ld failed\n", i); if (csv) csv_close(csv); return 1;
    }
  }

  /* Measured. */
  uint64_t cpu0 = cpu_ns();
  long mismatches = 0;
  uint64_t kg_sum = 0, en_sum = 0, de_sum = 0;
  for (long i = 0; i < count; i++) {
    uint64_t kg = 0, en = 0, de = 0; int m = 0;
    if (kem_roundtrip(&kg, &en, &de, &m) != 0) {
      fprintf(stderr, "measured cycle %ld hard error\n", i); if (csv) csv_close(csv); return 1;
    }
    if (!m) mismatches++;
    kg_sum += kg; en_sum += en; de_sum += de;
    if (csv) csv_row(csv, "%ld,%llu,%llu,%llu,%d", i,
                     (unsigned long long)kg, (unsigned long long)en,
                     (unsigned long long)de, m);
  }
  uint64_t cpu1 = cpu_ns();
  if (csv) csv_close(csv);

  /* Implicit rejection (TOR 8, local). */
  int differs = 0;
  if (kem_implicit_rejection_differs(&differs) != 0) {
    fprintf(stderr, "implicit-rejection check hard error\n"); return 1;
  }

  printf("--- summary ---\n");
  printf("cycles=%ld mismatches=%ld\n", count, mismatches);
  printf("mean_keygen_ns=%llu mean_encaps_ns=%llu mean_decaps_ns=%llu\n",
         (unsigned long long)(count ? kg_sum / (uint64_t)count : 0),
         (unsigned long long)(count ? en_sum / (uint64_t)count : 0),
         (unsigned long long)(count ? de_sum / (uint64_t)count : 0));
  printf("cpu_ns_total=%llu peak_rss_kb=%ld\n",
         (unsigned long long)(cpu1 - cpu0), peak_rss_kb());
  printf("implicit_rejection_differs=%d (expect 1)\n", differs);
  if (csv_path) printf("csv=%s\n", csv_path);

  if (mismatches != 0) { fprintf(stderr, "FAIL: %ld secret mismatches\n", mismatches); return 1; }
  if (!differs) { fprintf(stderr, "FAIL: implicit rejection did not differ\n"); return 1; }
  printf("RESULT: PASS\n");
  return 0;
}
