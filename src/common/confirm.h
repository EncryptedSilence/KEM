#ifndef PQC_CONFIRM_H
#define PQC_CONFIRM_H
#include <openssl/ssl.h>
#include <stddef.h>

#define PQC_EXPORT_LABEL "EXPERIMENTAL-pqc-orange-pi-v1"   /* length 29 */
#define PQC_KEY_LEN 32
#define PQC_ID_LEN  32

/* RFC 8446 exporter: derive 32 bytes with the fixed label and ctx32 as the
 * exporter context (use_context=1). Returns 1 on success, 0 on failure. */
int export_app_key(SSL *s, const unsigned char ctx32[PQC_ID_LEN],
                   unsigned char out32[PQC_KEY_LEN]);

/* tag = HMAC-SHA256(key32, label || id). */
void confirm_tag(const unsigned char key32[PQC_KEY_LEN], const char *label,
                 const unsigned char *id, size_t idlen,
                 unsigned char out32[32]);

/* Constant-time verify of a received tag. Returns 1 if valid, 0 otherwise. */
int confirm_verify(const unsigned char key32[PQC_KEY_LEN], const char *label,
                   const unsigned char *id, size_t idlen,
                   const unsigned char tag32[32]);

#endif /* PQC_CONFIRM_H */
