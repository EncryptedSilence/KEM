#ifndef PQC_ARGS_H
#define PQC_ARGS_H
/* Parsed pq_node command line. */
typedef struct {
  const char *role;        /* "server" | "client"            */
  const char *profile;     /* "baseline" | "pq"              */
  const char *mode;        /* "single" | "bench-net"         */
  const char *peer;        /* client: server IP (numeric)    */
  const char *bind;        /* server: listen IP (numeric)    */
  int port;
  int count;               /* measured connections           */
  int warmup;              /* warm-up connections            */
  const char *csv;         /* raw output path (optional)     */
  const char *ca;          /* CA cert path                   */
  const char *cert;        /* this node's cert               */
  const char *key;         /* this node's private key        */
  const char *expect_host; /* identity to verify in peer cert*/
  int corrupt_confirm;     /* negative test: tamper tag      */
  int drop_after_tcp;      /* negative test: close pre-TLS   */
  int stall_ms;            /* negative test: connect, hold, close (no TLS) */
  int timeout_ms;          /* per-connection I/O timeout     */
} Args;

/* Returns 0 on success, non-zero on bad usage (prints to stderr). */
int parse_args(int argc, char **argv, Args *out);

#endif /* PQC_ARGS_H */
