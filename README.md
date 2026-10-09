# PQC session-key experiment — PC ↔ Orange Pi R1

A reproducible experiment that establishes a shared TLS 1.3 session key between
a PC and an Orange Pi R1 using **post-quantum** key agreement
(ML-KEM-768 via the `X25519MLKEM768` hybrid group), built on **OpenSSL 3.5.9**
(3.5 LTS), and measures the cost of the post-quantum part versus a classical
`X25519` baseline.

It validates the **post-quantum component of key agreement only**;
authentication stays classical (ECDSA P-256). See [`docs/REPORT.md`](docs/REPORT.md)
for results and the full scope statement.

## What's here

```
src/common/     libcommon: net, timing, csv, version-assert, TLS ctx,
                exporter+HMAC confirmation, ML-KEM primitive
src/kem_bench/  local ML-KEM-768 micro-benchmark (TOR §4 + §8-local)
src/pq_node/    TLS 1.3 client/server, one binary, role/profile/mode by flags
tests/          unit tests (ctest): test_common, test_kem, test_confirm
scripts/        build, cert issuance, network setup, run & analysis scripts
cmake/          armhf cross toolchain file
docs/           REPORT, BUILD, RUN, ENVIRONMENT + spec & plan under superpowers/
results/        environment captures, raw CSVs, generated stats
```

## Design in one paragraph

Two TLS 1.3 profiles differing only in the key-exchange group (`X25519` vs
`X25519MLKEM768`), both `TLS_AES_256_GCM_SHA384`, mutual auth with a local-CA
ECDSA P-256 cert pair. Exactly one group is offered per profile (no fallback);
the negotiated group is verified to be literally the expected one. After the
handshake both sides derive a 32-byte application key with
`SSL_export_keying_material` (label `EXPERIMENTAL-pqc-orange-pi-v1`, a random
32-byte experiment id as context) and confirm agreement with mutual
HMAC-SHA-256. OpenSSL 3.5.9 is pinned (sha256-verified) and **statically
linked** so the binaries are self-contained and provably use the pinned library
(verified at runtime); the Pi binaries are cross-compiled on the PC for armhf.

## Quick start

1. Build pinned OpenSSL + app, issue certs — [`docs/BUILD.md`](docs/BUILD.md).
2. Set up the link and run the measurements — [`docs/RUN.md`](docs/RUN.md).
3. Read [`docs/REPORT.md`](docs/REPORT.md).

## Key facts

- OpenSSL **3.5.9**, sha256 `603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`.
- Orange Pi R1: Allwinner H2+, **32-bit ARMv7** (not AArch64), 484 MiB RAM, Armbian trixie.
- Measurement link: direct Ethernet, static `10.42.0.1` (PC) ↔ `10.42.0.2` (Pi).
- No private keys are committed (`certs/private/` is git-ignored).
