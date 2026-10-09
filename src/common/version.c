#include "version.h"
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *openssl_runtime_version(void) {
  return OpenSSL_version(OPENSSL_VERSION);
}

int openssl_version_has(const char *want) {
  if (!want) return 0;
  return strstr(openssl_runtime_version(), want) != NULL ? 1 : 0;
}

void assert_openssl_version(const char *want) {
  if (!openssl_version_has(want)) {
    fprintf(stderr, "FATAL: expected OpenSSL %s but linked runtime is '%s'\n",
            want ? want : "(null)", openssl_runtime_version());
    exit(3);
  }
}
