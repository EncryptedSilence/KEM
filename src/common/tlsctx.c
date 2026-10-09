#include "tlsctx.h"
#include <string.h>

const char *profile_group(const char *profile) {
  if (!profile) return NULL;
  if (strcmp(profile, "baseline") == 0) return "X25519";
  if (strcmp(profile, "pq") == 0) return "X25519MLKEM768";
  return NULL;
}

SSL_CTX *make_ctx(int is_server, const char *profile,
                  const char *ca, const char *cert, const char *key) {
  const char *group = profile_group(profile);
  if (!group) return NULL;                 /* group enforcement first */

  const SSL_METHOD *method = is_server ? TLS_server_method() : TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (!ctx) return NULL;

  /* TLS 1.3 only. */
  if (!SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) ||
      !SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION)) goto fail;

  /* Exactly one key-exchange group, no fallback. */
  if (SSL_CTX_set1_groups_list(ctx, group) != 1) goto fail;

  /* The single required TLS 1.3 ciphersuite. */
  if (SSL_CTX_set_ciphersuites(ctx, "TLS_AES_256_GCM_SHA384") != 1) goto fail;

  /* No session resumption / tickets / 0-RTT. */
  SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);
  SSL_CTX_set_num_tickets(ctx, 0);
  SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

  /* Mutual authentication against the local test CA. */
  if (SSL_CTX_load_verify_locations(ctx, ca, NULL) != 1) goto fail;
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
  SSL_CTX_set_verify_depth(ctx, 4);

  if (SSL_CTX_use_certificate_chain_file(ctx, cert) != 1) goto fail;
  if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) != 1) goto fail;
  if (SSL_CTX_check_private_key(ctx) != 1) goto fail;

  return ctx;

fail:
  SSL_CTX_free(ctx);
  return NULL;
}
