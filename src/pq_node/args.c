#include "args.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void usage(const char *p) {
  fprintf(stderr,
    "usage: %s --role server|client --profile baseline|pq [options]\n"
    "  --mode single|bench-net   (default single)\n"
    "  --peer IP                 (client: server numeric IP)\n"
    "  --bind IP                 (server: listen IP, default 0.0.0.0)\n"
    "  --port N                  (default 44330)\n"
    "  --count N --warmup N      (bench-net; defaults 1000/100)\n"
    "  --csv PATH                (bench-net raw output)\n"
    "  --ca FILE --cert FILE --key FILE\n"
    "  --expect-host NAME        (identity to require in peer cert)\n"
    "  --timeout-ms N            (default 5000)\n"
    "  --corrupt-confirm         (negative test: tamper the tag)\n"
    "  --drop-after-tcp          (negative test: close before TLS)\n",
    p);
}

int parse_args(int argc, char **argv, Args *o) {
  memset(o, 0, sizeof(*o));
  o->mode = "single";
  o->bind = "0.0.0.0";
  o->port = 44330;
  o->count = 1000;
  o->warmup = 100;
  o->timeout_ms = 5000;

  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    #define NEXT() (i + 1 < argc ? argv[++i] : NULL)
    if (!strcmp(a, "--role")) o->role = NEXT();
    else if (!strcmp(a, "--profile")) o->profile = NEXT();
    else if (!strcmp(a, "--mode")) o->mode = NEXT();
    else if (!strcmp(a, "--peer")) o->peer = NEXT();
    else if (!strcmp(a, "--bind")) o->bind = NEXT();
    else if (!strcmp(a, "--port")) o->port = atoi(NEXT() ?: "0");
    else if (!strcmp(a, "--count")) o->count = atoi(NEXT() ?: "0");
    else if (!strcmp(a, "--warmup")) o->warmup = atoi(NEXT() ?: "0");
    else if (!strcmp(a, "--csv")) o->csv = NEXT();
    else if (!strcmp(a, "--ca")) o->ca = NEXT();
    else if (!strcmp(a, "--cert")) o->cert = NEXT();
    else if (!strcmp(a, "--key")) o->key = NEXT();
    else if (!strcmp(a, "--expect-host")) o->expect_host = NEXT();
    else if (!strcmp(a, "--timeout-ms")) o->timeout_ms = atoi(NEXT() ?: "0");
    else if (!strcmp(a, "--corrupt-confirm")) o->corrupt_confirm = 1;
    else if (!strcmp(a, "--drop-after-tcp")) o->drop_after_tcp = 1;
    else if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(argv[0]); return 2; }
    else { fprintf(stderr, "unknown arg: %s\n", a); usage(argv[0]); return 1; }
    #undef NEXT
  }

  if (!o->role || (strcmp(o->role, "server") && strcmp(o->role, "client"))) {
    fprintf(stderr, "--role server|client required\n"); return 1;
  }
  if (!o->profile || (strcmp(o->profile, "baseline") && strcmp(o->profile, "pq"))) {
    fprintf(stderr, "--profile baseline|pq required\n"); return 1;
  }
  if (!strcmp(o->role, "client") && !o->peer) {
    fprintf(stderr, "client needs --peer IP\n"); return 1;
  }
  if (!o->ca || !o->cert || !o->key) {
    fprintf(stderr, "--ca --cert --key required\n"); return 1;
  }
  return 0;
}
