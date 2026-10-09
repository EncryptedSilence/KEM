# Build instructions

Everything builds on the PC. The Orange Pi needs **no** toolchain — its armhf
binaries are cross-compiled on the PC and copied over. On Windows, all PC-side
commands run inside **WSL2** (Ubuntu). Commands below are run from the repo root
inside WSL (`cd /mnt/d/CPP/KEM` or your path).

No `sudo` is required: the build fetches a portable CMake and a self-contained
armhf cross-toolchain into `.tools/` (git-ignored).

## 0. One-time tooling (no sudo)

```bash
bash scripts/get-cmake.sh          # portable CMake into .tools/cmake
bash scripts/get-cross.sh          # Bootlin armv7 cross-toolchain into .tools/cross-armhf
                                   # (prints the absolute compiler prefix)
```

## 1. Pinned OpenSSL 3.5.9 (static, per arch)

Downloads `openssl-3.5.9.tar.gz`, verifies its sha256
(`603f5602…859a`), and builds static libs into `install/<arch>/openssl`.

```bash
# PC (x86_64), native — also builds bin/openssl used for cert issuance
bash scripts/build-openssl.sh x86_64

# Orange Pi (armhf), cross-compiled
export PQ_CROSS_PREFIX="$(bash scripts/get-cross.sh)"
bash scripts/build-openssl.sh armhf
```

Verify: `install/x86_64/openssl/bin/openssl version` prints `OpenSSL 3.5.9`;
`file install/armhf/openssl/bin/openssl` shows `ELF 32-bit … ARM`.

## 2. Application (static binaries)

```bash
# PC
bash scripts/build-all.sh x86_64            # -> build/x86_64/{pq_node,kem_bench,test_*}

# Orange Pi (armhf), cross-compiled (fully static)
export PQ_CROSS_PREFIX="$(bash scripts/get-cross.sh)"
bash scripts/build-all.sh armhf             # -> build/armhf/{pq_node,kem_bench}
```

`file build/armhf/pq_node` → `ELF 32-bit … ARM, statically linked`. Static
linking means a glibc-version mismatch between PC (2.43) and Pi (2.41) does not
matter, and the binary is provably bound to the pinned OpenSSL 3.5.9.

## 3. Unit tests

```bash
bash scripts/run-tests.sh x86_64            # ctest: test_common, test_kem, test_confirm
```

## 4. Test certificates (local CA, ECDSA P-256)

```bash
bash scripts/make-certs.sh                  # -> certs/{ca,pc-node,opi-node,rogue}.crt
                                            #    keys in certs/private/ (git-ignored)
```

## Notes

- Benign linker warnings about `getaddrinfo`/`gethostbyname` in statically
  linked applications come from OpenSSL's `libcrypto`; the data path uses
  numeric IPs only, so no name resolution occurs at runtime.
- To rebuild cleanly, remove `build/<arch>` and re-run `build-all.sh`.
