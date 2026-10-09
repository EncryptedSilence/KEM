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
#include <sys/socket.h>
#include <sys/time.h>

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
  /* Bound every socket read/write so a stalled peer cannot hang the handshake
   * (also what makes the timeout negative test deterministic). */
  if (a->timeout_ms > 0) {
    struct timeval tv = { a->timeout_ms / 1000, (a->timeout_ms % 1000) * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  }
  SSL *ssl = SSL_new(ctx);
  if (!ssl) { close(fd); return -1; }
  SSL_set_fd(ssl, fd);
  if (a->expect_host) SSL_set1_host(ssl, a->expect_host);

  uint64_t h0 = mono_ns();
  int hs = is_server ? SSL_accept(ssl) : SSL_connect(ssl);
  uint64_t h1 = mono_ns();
  /* SSL_set_fd uses BIO_NOCLOSE, so SSL_free never closes the socket — we own
   * fd and must close it on every path, or we leak one fd per connection. */
  if (hs != 1) { SSL_free(ssl); close(fd); return -2; }
  r->handshake_ns = h1 - h0;

  /* Handshake byte counts at the socket BIO (TLS-record level). */
  r->bytes_in = (long)BIO_number_read(SSL_get_rbio(ssl));
  r->bytes_out = (long)BIO_number_written(SSL_get_wbio(ssl));

  if (check_params(ssl, a->profile, r) != 0) { SSL_free(ssl); close(fd); return -3; }

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
  close(fd);
  return 0;

cfail:
  SSL_free(ssl);
  close(fd);
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
    if (a.stall_ms > 0) { usleep((useconds_t)a.stall_ms * 1000); close(fd); fprintf(stderr, "client stalled %dms then closed (negative test)\n", a.stall_ms); SSL_CTX_free(ctx); return 0; }
    ConnResult r;
    int cr = handle_conn(ctx, fd, 0, &a, &r);
    if (cr == 0) { print_result(0, &a, &r); } else { fprintf(stderr, "client connection failed rc=%d\n", cr); rc = 1; }
  }

  SSL_CTX_free(ctx);
  return rc;
}

/* bench-net: run warmup+count full connections, record per-connection metrics
 * on this node's own monotonic clock. Each connection is fresh (no resumption).
 * CSV columns include a phase flag (0=warmup, 1=measured) for the analyzer. */
#define ULL(x) ((unsigned long long)(x))
static const char *CSV_HEADER =
  "conn,phase,tcp_connect_ns,tls_handshake_ns,keyconfirm_ns,cpu_ns,"
  "bytes_in,bytes_out,peak_rss_kb,group,tls_version,cipher,resumed,status";

static int run_bench_net(SSL_CTX *ctx, const Args *a, int is_server) {
  long total = (long)a->warmup + (long)a->count;
  Csv *csv = NULL;
  if (a->csv) {
    csv = csv_open(a->csv, CSV_HEADER);
    if (!csv) { fprintf(stderr, "cannot open csv %s\n", a->csv); return 1; }
  }

  long ok = 0, err = 0, tmo = 0;
  int consec_timeouts = 0;
  int lfd = -1;
  if (is_server) {
    lfd = tcp_listen(a->bind, a->port);
    if (lfd < 0) { fprintf(stderr, "listen %s:%d failed\n", a->bind, a->port); if (csv) csv_close(csv); return 1; }
    fprintf(stderr, "server bench on %s:%d profile=%s total=%ld\n", a->bind, a->port, a->profile, total);
  }

  for (long i = 0; i < total; i++) {
    int phase = (i < a->warmup) ? 0 : 1;
    ConnResult r;
    memset(&r, 0, sizeof r);
    uint64_t tconn = 0;
    int fd;

    if (is_server) {
      /* Tolerate transient idle: keep listening across short gaps so a single
       * slow/failed connection cannot make the server exit mid-run and refuse
       * all the client's remaining connects. Conclude the client is done only
       * after several consecutive idle windows. */
      fd = tcp_accept(lfd, 5000);
      if (fd < 0) {
        if (++consec_timeouts >= 4) {
          fprintf(stderr, "client idle — server stopping at conn %ld\n", i);
          break;
        }
        i--;              /* this slot was not a real connection; retry it */
        continue;
      }
      consec_timeouts = 0;
    } else {
      uint64_t t0 = mono_ns();
      fd = tcp_connect(a->peer, a->port, a->timeout_ms);
      tconn = mono_ns() - t0;
      if (fd < 0) {
        err++;
        if (csv) csv_row(csv, "%ld,%d,%llu,0,0,0,0,0,%ld,-,-,-,-,connect_err", i, phase, ULL(tconn), peak_rss_kb());
        continue;
      }
    }

    uint64_t cpu0 = cpu_ns();
    int cr = handle_conn(ctx, fd, is_server, a, &r);
    uint64_t cpu1 = cpu_ns();

    if (cr == 0 && r.ok) {
      ok++;
      if (csv) csv_row(csv, "%ld,%d,%llu,%llu,%llu,%llu,%ld,%ld,%ld,%s,%s,%s,%d,ok",
                       i, phase, ULL(tconn), ULL(r.handshake_ns), ULL(r.keyconfirm_ns),
                       ULL(cpu1 - cpu0), r.bytes_in, r.bytes_out, peak_rss_kb(),
                       r.group, r.version ? r.version : "-", r.cipher ? r.cipher : "-", r.resumed);
    } else {
      err++;
      if (csv) csv_row(csv, "%ld,%d,%llu,0,0,%llu,0,0,%ld,-,-,-,-,err_rc%d",
                       i, phase, ULL(tconn), ULL(cpu1 - cpu0), peak_rss_kb(), cr);
    }
  }

  if (lfd >= 0) close(lfd);
  if (csv) csv_close(csv);

  fprintf(stderr, "bench %s done: ok=%ld err=%ld timeout=%ld (warmup=%d count=%d)\n",
          is_server ? "server" : "client", ok, err, tmo, a->warmup, a->count);
  printf("BENCH_SUMMARY role=%s profile=%s ok=%ld err=%ld timeout=%ld peak_rss_kb=%ld\n",
         a->role, a->profile, ok, err, tmo, peak_rss_kb());

  return (err == 0 && tmo == 0) ? 0 : 1;
}
