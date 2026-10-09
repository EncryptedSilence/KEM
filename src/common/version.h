#ifndef PQC_VERSION_H
#define PQC_VERSION_H
/* Abort (exit 3) unless the linked OpenSSL runtime version string contains
 * `want` (e.g. "3.5.9"). Proves the pinned library is in use. */
void assert_openssl_version(const char *want);
#endif /* PQC_VERSION_H */
