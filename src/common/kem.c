#include "kem.h"
#include "timing.h"

#include <openssl/evp.h>
#include <openssl/crypto.h>

int kem_init_once(uint64_t *init_ns) {
  uint64_t t0 = mono_ns();
  EVP_PKEY *p = EVP_PKEY_Q_keygen(NULL, NULL, KEM_ALG);
  uint64_t t1 = mono_ns();
  if (!p) return 1;
  EVP_PKEY_free(p);
  if (init_ns) *init_ns = t1 - t0;
  return 0;
}

int kem_roundtrip(uint64_t *keygen_ns, uint64_t *encaps_ns,
                  uint64_t *decaps_ns, int *match) {
  int ret = 1;
  EVP_PKEY *pkey = NULL;
  EVP_PKEY_CTX *ectx = NULL, *dctx = NULL;
  unsigned char *ct = NULL, *ss1 = NULL, *ss2 = NULL;
  size_t ctlen = 0, sslen = 0, sslen2 = 0;

  uint64_t t0 = mono_ns();
  pkey = EVP_PKEY_Q_keygen(NULL, NULL, KEM_ALG);   /* fresh keypair */
  uint64_t t1 = mono_ns();
  if (!pkey) goto done;
  if (keygen_ns) *keygen_ns = t1 - t0;

  /* --- encapsulate (public key) --- */
  ectx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
  if (!ectx || EVP_PKEY_encapsulate_init(ectx, NULL) <= 0) goto done;
  if (EVP_PKEY_encapsulate(ectx, NULL, &ctlen, NULL, &sslen) <= 0) goto done;
  ct = OPENSSL_malloc(ctlen);
  ss1 = OPENSSL_malloc(sslen);
  if (!ct || !ss1) goto done;
  {
    uint64_t t2 = mono_ns();
    if (EVP_PKEY_encapsulate(ectx, ct, &ctlen, ss1, &sslen) <= 0) goto done;
    uint64_t t3 = mono_ns();
    if (encaps_ns) *encaps_ns = t3 - t2;
  }

  /* --- decapsulate (private key) --- */
  dctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
  if (!dctx || EVP_PKEY_decapsulate_init(dctx, NULL) <= 0) goto done;
  sslen2 = sslen;
  ss2 = OPENSSL_malloc(sslen2);
  if (!ss2) goto done;
  {
    uint64_t t4 = mono_ns();
    if (EVP_PKEY_decapsulate(dctx, ss2, &sslen2, ct, ctlen) <= 0) goto done;
    uint64_t t5 = mono_ns();
    if (decaps_ns) *decaps_ns = t5 - t4;
  }

  if (match)
    *match = (sslen2 == sslen && CRYPTO_memcmp(ss1, ss2, sslen) == 0) ? 1 : 0;
  ret = 0;

done:
  OPENSSL_free(ct);
  OPENSSL_free(ss1);
  OPENSSL_free(ss2);
  EVP_PKEY_CTX_free(ectx);
  EVP_PKEY_CTX_free(dctx);
  EVP_PKEY_free(pkey);
  return ret;
}

int kem_implicit_rejection_differs(int *differs) {
  int ret = 1;
  EVP_PKEY *pkey = EVP_PKEY_Q_keygen(NULL, NULL, KEM_ALG);
  EVP_PKEY_CTX *ectx = NULL, *dctx = NULL;
  unsigned char *ct = NULL, *ss1 = NULL, *ss2 = NULL;
  size_t ctlen = 0, sslen = 0, sslen2 = 0;
  if (!pkey) goto done;

  ectx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
  if (!ectx || EVP_PKEY_encapsulate_init(ectx, NULL) <= 0) goto done;
  if (EVP_PKEY_encapsulate(ectx, NULL, &ctlen, NULL, &sslen) <= 0) goto done;
  ct = OPENSSL_malloc(ctlen);
  ss1 = OPENSSL_malloc(sslen);
  if (!ct || !ss1) goto done;
  if (EVP_PKEY_encapsulate(ectx, ct, &ctlen, ss1, &sslen) <= 0) goto done;

  /* Flip one byte but keep the correct length -> implicit rejection. */
  ct[0] ^= 0x01;

  dctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
  if (!dctx || EVP_PKEY_decapsulate_init(dctx, NULL) <= 0) goto done;
  sslen2 = sslen;
  ss2 = OPENSSL_malloc(sslen2);
  if (!ss2) goto done;

  if (EVP_PKEY_decapsulate(dctx, ss2, &sslen2, ct, ctlen) <= 0) {
    /* An explicit API error also counts as rejection. */
    if (differs) *differs = 1;
    ret = 0;
    goto done;
  }
  /* The normal ML-KEM case: no error, but a different secret. */
  if (differs)
    *differs = (sslen2 == sslen && CRYPTO_memcmp(ss1, ss2, sslen) == 0) ? 0 : 1;
  ret = 0;

done:
  OPENSSL_free(ct);
  OPENSSL_free(ss1);
  OPENSSL_free(ss2);
  EVP_PKEY_CTX_free(ectx);
  EVP_PKEY_CTX_free(dctx);
  EVP_PKEY_free(pkey);
  return ret;
}
