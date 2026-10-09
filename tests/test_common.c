/* Tests for net / timing / csv (Task 2). Dependency-free harness. */
#include "net.h"
#include "timing.h"
#include "csv.h"
#include "version.h"
#include "tlsctx.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); failures++; } } while (0)

static void test_connect_rejects_nonnumeric(void) {
  /* numeric-only path: a hostname must be rejected, not resolved */
  int fd = tcp_connect("pc-node", 48000, 200);
  CHECK(fd == -1, "tcp_connect rejects non-numeric host");
}

static void test_loopback_roundtrip(void) {
  int lfd = tcp_listen("127.0.0.1", 0);
  CHECK(lfd >= 0, "tcp_listen on 127.0.0.1:0");
  int port = tcp_port(lfd);
  CHECK(port > 0, "tcp_port returns bound port");

  int cfd = tcp_connect("127.0.0.1", port, 1000);
  CHECK(cfd >= 0, "tcp_connect to listener");

  int afd = tcp_accept(lfd, 1000);
  CHECK(afd >= 0, "tcp_accept returns a connection");

  if (cfd >= 0 && afd >= 0) {
    char out = 'Z', in = 0;
    ssize_t w = write(cfd, &out, 1);
    ssize_t r = read(afd, &in, 1);
    CHECK(w == 1 && r == 1 && in == 'Z', "one byte round-trips client->server");
  } else {
    CHECK(0, "one byte round-trips client->server");
  }
  if (afd >= 0) close(afd);
  if (cfd >= 0) close(cfd);
  if (lfd >= 0) close(lfd);
}

static void test_accept_timeout(void) {
  int lfd = tcp_listen("127.0.0.1", 0);
  CHECK(lfd >= 0, "tcp_listen for timeout test");
  uint64_t t0 = mono_ns();
  int afd = tcp_accept(lfd, 150);       /* no client connects */
  uint64_t dt = mono_ns() - t0;
  CHECK(afd == -1, "tcp_accept times out with no client");
  CHECK(dt >= 100ull * 1000 * 1000, "tcp_accept waited ~the timeout");
  if (afd >= 0) close(afd);
  if (lfd >= 0) close(lfd);
}

static void test_mono_monotonic(void) {
  uint64_t a = mono_ns();
  uint64_t b = mono_ns();
  CHECK(a != 0, "mono_ns is nonzero");
  CHECK(b >= a, "mono_ns is non-decreasing");
  /* strictly advances across a small busy loop */
  volatile unsigned long acc = 0;
  for (unsigned long i = 0; i < 2000000ul; i++) acc += i;
  (void)acc;
  uint64_t c = mono_ns();
  CHECK(c > a, "mono_ns advances over work");
}

static void test_cpu_and_rss(void) {
  uint64_t c0 = cpu_ns();
  volatile unsigned long acc = 0;
  for (unsigned long i = 0; i < 5000000ul; i++) acc += i;
  (void)acc;
  uint64_t c1 = cpu_ns();
  CHECK(c1 >= c0, "cpu_ns is non-decreasing");
  CHECK(peak_rss_kb() > 0, "peak_rss_kb is positive");
}

static void test_csv(void) {
  const char *path = "/tmp/pqc_test_common.csv";
  unlink(path);
  Csv *c = csv_open(path, "a,b,c");
  CHECK(c != NULL, "csv_open succeeds");
  if (c) {
    csv_row(c, "%d,%s,%d", 1, "hello", 42);
    csv_row(c, "%d,%s,%d", 2, "world", 99);
    csv_close(c);
  }
  FILE *f = fopen(path, "r");
  CHECK(f != NULL, "csv file exists");
  if (f) {
    char buf[256]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); buf[n] = 0;
    fclose(f);
    CHECK(strstr(buf, "a,b,c\n") == buf, "header is first line");
    CHECK(strstr(buf, "1,hello,42\n") != NULL, "row 1 present");
    CHECK(strstr(buf, "2,world,99\n") != NULL, "row 2 present");
  } else {
    CHECK(0, "header is first line");
    CHECK(0, "row 1 present");
    CHECK(0, "row 2 present");
  }
}

static void test_version(void) {
  CHECK(openssl_version_has("3.5.9") == 1, "runtime OpenSSL is 3.5.9");
  CHECK(openssl_version_has("9.9.9") == 0, "wrong version not matched");
  CHECK(strstr(openssl_runtime_version(), "3.5.9") != NULL,
        "runtime version string contains 3.5.9");
  /* If assert_openssl_version(3.5.9) wrongly exited, we'd never return here. */
  assert_openssl_version("3.5.9");
  CHECK(1, "assert_openssl_version(3.5.9) does not abort");
}

static void test_profile_group(void) {
  CHECK(profile_group("baseline") && strcmp(profile_group("baseline"), "X25519") == 0,
        "profile baseline -> X25519");
  CHECK(profile_group("pq") && strcmp(profile_group("pq"), "X25519MLKEM768") == 0,
        "profile pq -> X25519MLKEM768");
  CHECK(profile_group("nonsense") == NULL, "unknown profile -> NULL");
}

static void test_make_ctx_rejects_bad_group(void) {
  /* Group enforcement happens before cert loading, so a bad profile returns
   * NULL regardless of cert paths. */
  SSL_CTX *c = make_ctx(1, "nonsense", "none", "none", "none");
  CHECK(c == NULL, "make_ctx rejects unknown profile");
  if (c) SSL_CTX_free(c);
}

int main(void) {
  printf("== test_common ==\n");
  test_connect_rejects_nonnumeric();
  test_loopback_roundtrip();
  test_accept_timeout();
  test_mono_monotonic();
  test_cpu_and_rss();
  test_csv();
  test_version();
  test_profile_group();
  test_make_ctx_rejects_bad_group();
  printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
