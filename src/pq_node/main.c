/* pq_node — TLS 1.3 client/server for the baseline-vs-PQ experiment.
 * One binary, role/profile/mode by flags. This file: handshake core, the
 * per-connection flow (checks + key confirmation + data exchange), single
 * mode, and bench-net mode (measured loop -> CSV). */
#include "args.h"
#include "tlsctx.h"
#include "confirm.h"
#include "net.h"
#include "timing.h"
#include "csv.h"
#include "version.h"

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

typedef struct {
  uint64_t handshake_ns;
  uint64_t keyconfirm_ns;
  long bytes_in;
  long bytes_out;
  char group[64];
  const char *version;
  const char *cipher;
  int resumed;
  int ok;             /* 1 = full success */
} ConnResult;

static int read_full(SSL *s, void *buf, size_t n) {
  size_t got = 0;
  while (got < n) {
    size_t r = 0;
    if (SSL_read_ex(s, (char *)buf + got, n - got, &r) <= 0) return -1;
    got += r;
  }
  return 0;
}

static int write_full(SSL *s, const void *buf, size_t n) {
  size_t put = 0;
  while (put < n) {
    size_t w = 0;
    if (SSL_write_ex(s, (const char *)buf + put, n - put, &w) <= 0) return -1;
    put += w;
  }
  return 0;
}

/* Verify and record the negotiated parameters. Returns 0 if all required
 * values hold (correct group, TLS 1.3, required cipher, not resumed). */
static int check_params(SSL *s, const char *profile, ConnResult *r) {
  const char *neg = SSL_get0_group_name(s);
  const char *want = profile_group(profile);
  r->version = SSL_get_version(s);
  r->cipher = SSL_get_cipher(s);
  r->resumed = SSL_session_reused(s);
  snprintf(r->group, sizeof(r->group), "%s", neg ? neg : "(none)");

  if (!neg || !want || strcasecmp(neg, want) != 0) {
    fprintf(stderr, "FAIL group: negotiated=%s want=%s\n", neg ? neg : "(none)", want ? want : "?");
    return -1;
  }
  if (!r->version || strcmp(r->version, "TLSv1.3") != 0) {
    fprintf(stderr, "FAIL version: %s\n", r->version ? r->version : "(none)"); return -1;
  }
  if (!r->cipher || strcmp(r->cipher, "TLS_AES_256_GCM_SHA384") != 0) {
    fprintf(stderr, "FAIL cipher: %s\n", r->cipher ? r->cipher : "(none)"); return -1;
  }
  if (r->resumed != 0) { fprintf(stderr, "FAIL session was resumed\n"); return -1; }
  return 0;
}

/* Full per-connection flow over an accepted/connected socket `fd`.
 * Returns 0 on success; negative on failure (handshake/params/confirm). */
static int handle_conn(SSL_CTX *ctx, int fd, int is_server, const Args *a, ConnResult *r) {
  memset(r, 0, sizeof(*r));
  SSL *ssl = SSL_new(ctx);
  if (!ssl) { close(fd); return -1; }
  SSL_set_fd(ssl, fd);
  if (a->expect_host) SSL_set1_host(ssl, a->expect_host);

  uint64_t h0 = mono_ns();
  int hs = is_server ? SSL_accept(ssl) : SSL_connect(ssl);
  uint64_t h1 = mono_ns();
  if (hs != 1) { SSL_free(ssl); return -2; }
  r->handshake_ns = h1 - h0;

  /* Handshake byte counts at the socket BIO (TLS-record level). */
  r->bytes_in = (long)BIO_number_read(SSL_get_rbio(ssl));
  r->bytes_out = (long)BIO_number_written(SSL_get_wbio(ssl));

  if (check_params(ssl, a->profile, r) != 0) { SSL_free(ssl); return -3; }

  unsigned char id[PQC_ID_LEN], key[PQC_KEY_LEN], tag_peer[32], tag_self[32];
  char data[4];
  uint64_t c0 = mono_ns();
  if (is_server) {
    if (read_full(ssl, id, sizeof id) != 0) goto cfail;
    if (read_full(ssl, tag_peer, 32) != 0) goto cfail;
    if (!export_app_key(ssl, id, key)) goto cfail;
    if (!confirm_verify(key, "client-confirm", id, sizeof id, tag_peer)) {
      fprintf(stderr, "FAIL client-confirm mismatch\n"); goto cfail;
    }
    confirm_tag(key, "server-confirm", id, sizeof id, tag_self);
    if (write_full(ssl, tag_self, 32) != 0) goto cfail;
    if (read_full(ssl, data, 4) != 0) goto cfail;      /* "PING" */
    if (write_full(ssl, "PONG", 4) != 0) goto cfail;
  } else {
    if (RAND_bytes(id, sizeof id) != 1) goto cfail;
    if (!export_app_key(ssl, id, key)) goto cfail;
    confirm_tag(key, "client-confirm", id, sizeof id, tag_self);
    if (a->corrupt_confirm) tag_self[0] ^= 0x01;        /* negative test */
    if (write_full(ssl, id, sizeof id) != 0) goto cfail;
    if (write_full(ssl, tag_self, 32) != 0) goto cfail;
    if (read_full(ssl, tag_peer, 32) != 0) goto cfail;
    if (!confirm_verify(key, "server-confirm", id, sizeof id, tag_peer)) {
      fprintf(stderr, "FAIL server-confirm mismatch\n"); goto cfail;
    }
    if (write_full(ssl, "PING", 4) != 0) goto cfail;
    if (read_full(ssl, data, 4) != 0) goto cfail;       /* "PONG" */
  }
  r->keyconfirm_ns = mono_ns() - c0;
  r->ok = 1;
  SSL_shutdown(ssl);
  SSL_free(ssl);
  return 0;

cfail:
  SSL_free(ssl);
  return -4;
}

static void print_result(int is_server, const Args *a, const ConnResult *r) {
  printf("%s PASS profile=%s group=%s version=%s cipher=%s resumed=%d "
         "handshake_ns=%llu keyconfirm_ns=%llu bytes_in=%ld bytes_out=%ld peak_rss_kb=%ld\n",
         is_server ? "server" : "client", a->profile, r->group, r->version,
         r->cipher, r->resumed,
         (unsigned long long)r->handshake_ns, (unsigned long long)r->keyconfirm_ns,
         r->bytes_in, r->bytes_out, peak_rss_kb());
}

/* ----- bench-net (Task 7) -------------------------------------------------*/
static int run_bench_net(SSL_CTX *ctx, const Args *a, int is_server);

int main(int argc, char **argv) {
  Args a;
  int pr = parse_args(argc, argv, &a);
  if (pr != 0) return pr == 2 ? 0 : 2;

  assert_openssl_version("3.5.9");
  int is_server = !strcmp(a.role, "server");

  SSL_CTX *ctx = make_ctx(is_server, a.profile, a.ca, a.cert, a.key);
  if (!ctx) {
    fprintf(stderr, "FATAL: make_ctx failed for profile '%s' (group '%s' unavailable or bad certs)\n",
            a.profile, profile_group(a.profile) ? profile_group(a.profile) : "?");
    return 2;
  }

  int rc = 0;
  if (!strcmp(a.mode, "bench-net")) {
    rc = run_bench_net(ctx, &a, is_server);
    SSL_CTX_free(ctx);
    return rc;
  }

  /* --- single mode --- */
  if (is_server) {
    int lfd = tcp_listen(a.bind, a.port);
    if (lfd < 0) { fprintf(stderr, "listen %s:%d failed\n", a.bind, a.port); SSL_CTX_free(ctx); return 1; }
    fprintf(stderr, "server listening on %s:%d profile=%s\n", a.bind, a.port, a.profile);
    int fd = tcp_accept(lfd, a.timeout_ms > 0 ? a.timeout_ms * 20 : 60000);
    close(lfd);
    if (fd < 0) { fprintf(stderr, "accept timeout\n"); SSL_CTX_free(ctx); return 1; }
    ConnResult r;
    int cr = handle_conn(ctx, fd, 1, &a, &r);
    if (cr == 0) { print_result(1, &a, &r); } else { fprintf(stderr, "server connection failed rc=%d\n", cr); rc = 1; }
  } else {
    int fd = tcp_connect(a.peer, a.port, a.timeout_ms);
    if (fd < 0) { fprintf(stderr, "connect %s:%d failed\n", a.peer, a.port); SSL_CTX_free(ctx); return 1; }
    if (a.drop_after_tcp) { close(fd); fprintf(stderr, "client dropped after TCP (negative test)\n"); SSL_CTX_free(ctx); return 0; }
    ConnResult r;
    int cr = handle_conn(ctx, fd, 0, &a, &r);
    if (cr == 0) { print_result(0, &a, &r); } else { fprintf(stderr, "client connection failed rc=%d\n", cr); rc = 1; }
  }

  SSL_CTX_free(ctx);
  return rc;
}

/* bench-net implemented in Task 7 */
static int run_bench_net(SSL_CTX *ctx, const Args *a, int is_server) {
  (void)ctx; (void)a; (void)is_server;
  fprintf(stderr, "bench-net not implemented yet\n");
  return 3;
}
