#ifndef PQC_TLSCTX_H
#define PQC_TLSCTX_H
#include <openssl/ssl.h>

/* "baseline" -> "X25519", "pq" -> "X25519MLKEM768", else NULL. */
const char *profile_group(const char *profile);

/* Build a TLS 1.3-only SSL_CTX for the given profile:
 *  - exactly one key-exchange group (profile_group), no fallback
 *  - cipher TLS_AES_256_GCM_SHA384
 *  - mutual auth: verify peer against `ca`, load `cert`/`key`
 *  - resumption + tickets disabled
 * Returns NULL on any failure (unknown group, missing files, ...). */
SSL_CTX *make_ctx(int is_server, const char *profile,
                  const char *ca, const char *cert, const char *key);

#endif /* PQC_TLSCTX_H */
