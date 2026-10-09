#ifndef PQC_VERSION_H
#define PQC_VERSION_H
/* 1 if the linked OpenSSL runtime version string contains `want`, else 0. */
int openssl_version_has(const char *want);

/* The runtime OpenSSL version string, e.g. "OpenSSL 3.5.9 29 Sep 2026". */
const char *openssl_runtime_version(void);

/* Abort (exit 3) unless openssl_version_has(want). Proves the pinned lib. */
void assert_openssl_version(const char *want);
#endif /* PQC_VERSION_H */
