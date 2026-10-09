# PQC ML-KEM TLS Experiment — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (native, chosen by user) to implement task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Build a reproducible experiment that runs ML-KEM-768 locally and X25519MLKEM768 over TLS 1.3 between a PC (WSL2) and an Orange Pi R1, measuring the cost of the post-quantum key agreement vs a classical baseline.

**Architecture:** One C17/CMake project → `libcommon` + `kem_bench` + `pq_node`. OpenSSL 3.5.9 built from source (sha256-pinned) per arch, **statically** linked. x86_64 built natively in WSL; armhf cross-built in WSL and rsync'd to the Pi. A static-IP direct Ethernet link carries the measured traffic; bootstrap/control is SSH.

**Tech Stack:** C17, CMake, OpenSSL 3.5.9 (EVP, TLS 1.3, exporter, HMAC), bash, PowerShell (net setup), python3 (analysis).

**Spec:** `docs/superpowers/specs/2026-10-09-pqc-orange-pi-design.md`

## Global Constraints

- OpenSSL **3.5.9** only; sha256 `603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`.
- Binaries **fully static** (`-static`), OpenSSL built `no-shared no-dso no-tests`.
- Runtime asserts `OpenSSL_version(OPENSSL_VERSION)` contains `3.5.9`; exit non-zero otherwise.
- TLS **1.3 only**; cipher `TLS_AES_256_GCM_SHA384`; **exactly one** group per profile (`X25519` | `X25519MLKEM768`), no fallback.
- Mutual auth, local-CA ECDSA P-256; verify chain + peer identity.
- Resumption + 0-RTT **off**; assert `SSL_session_reused()==0` every connection.
- Exporter: 32 bytes, label `EXPERIMENTAL-pqc-orange-pi-v1` (len 29), context = client's 32-byte experiment ID, `use_context=1`.
- Confirmation: HMAC-SHA-256 keyed by exported key, over exp_id, labels `client-confirm`/`server-confirm`. Exported key/TLS secrets never logged or sent.
- 100 warm-up + 1000 measured for KEM cycles (fresh keypair each) and network connections.
- Monotonic local clocks only; never subtract timestamps across devices.
- No private keys in git. Numeric IPs only on the data path (`AI_NUMERICHOST`).
- Docs/report in English.

## Review Focus

- **System OpenSSL shadowing the pinned one** → runtime version assert + static link (Task 3, `version.c`).
- **PQ group silently downgrading to classical** → accept PQ run only if negotiated group is literally `X25519MLKEM768`; single-group list makes mismatch a hard failure (Task 6).
- **Exporter label/context mismatch between sides** → both sides use identical label+context; a mismatch must make confirmation fail, tested in Task 7.
- **Static glibc + name resolution hang/crash** → `getaddrinfo` with `AI_NUMERICHOST|AI_NUMERICSERV`, tested to reject non-numeric (Task 2).
- **Implicit rejection mis-tested** (expecting an API error) → ciphertext-corruption test asserts *secrets differ*, not an error return (Task 4).

---

### Task 0: Networking + toolchain bring-up (host/Pi infra)

**Files:** Create `scripts/setup-net-pc.ps1`, `scripts/setup-net-pi.sh`, `scripts/setup-wsl-mirrored.ps1`, `scripts/pi-ssh.sh` (helper wrapper).

- [ ] Write `.wslconfig` mirrored-mode setter + `wsl --shutdown`; verify after restart that WSL sees the Windows Ethernet IP.
- [ ] PowerShell: set Windows Ethernet (ifIndex 22) static `10.42.0.1/24` (elevated); add firewall inbound allow for the test port.
- [ ] Pi: detect the cabled iface (the one holding `fe80::c274:2bff:feff:95f8`), `ip addr add 10.42.0.2/24`, add `/etc/hosts` entries `10.42.0.1 pc-node` / `10.42.0.2 opi-node` on both nodes.
- [ ] Install `gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf` on WSL (user sudo).
- [ ] **Verify:** `ping 10.42.0.2` from WSL and `ping 10.42.0.1` from Pi both 0% loss; `arm-linux-gnueabihf-gcc --version` works.
- [ ] Commit scripts.

### Task 1: OpenSSL 3.5.9 build scripts (both arches)

**Files:** Create `scripts/build-openssl.sh`, `cmake/armhf-toolchain.cmake`.

- [ ] `build-openssl.sh <x86_64|armhf>`: download `openssl-3.5.9.tar.gz` (cache in `3rdparty/openssl/`), `sha256sum -c` against the pinned hash (abort on mismatch), extract to `3rdparty/openssl/src-<arch>`, Configure + build + `install_sw` into `install/<arch>/openssl`.
  - x86_64: `./Configure linux-x86_64 no-shared no-dso no-tests no-docs threads --prefix=... --openssldir=.../ssl`
  - armhf: `./Configure linux-armv4 no-shared no-dso no-tests no-docs threads --cross-compile-prefix=arm-linux-gnueabihf- --prefix=...`
- [ ] **Verify:** `install/<arch>/openssl/lib*/libssl.a` and `libcrypto.a` exist; `install/x86_64/openssl/bin/openssl version` prints `OpenSSL 3.5.9`; `file install/armhf/openssl/bin/openssl` shows `ARM`.
- [ ] Commit.

### Task 2: CMake skeleton + `net` + `timing` + `csv` (libcommon part 1)

**Files:** Create `CMakeLists.txt`, `src/common/{net.c,net.h,timing.c,timing.h,csv.c,csv.h}`, `tests/test_common.c`.

**Interfaces produced:**
- `int tcp_listen(const char *ip, int port);` `int tcp_accept(int lfd, int timeout_ms);` `int tcp_connect(const char *ip, int port, int timeout_ms);` (all numeric-only, `AI_NUMERICHOST|AI_NUMERICSERV`; return -1 on error/timeout).
- `uint64_t mono_ns(void);` `uint64_t cpu_ns(void);` `long peak_rss_kb(void);`
- `typedef struct Csv Csv; Csv *csv_open(const char*, const char *header); void csv_row(Csv*, const char *fmt, ...); void csv_close(Csv*);`

- [ ] CMake: C17, `OPENSSL_ROOT_DIR=${CMAKE_SOURCE_DIR}/install/${PQ_ARCH}/openssl`, `OPENSSL_USE_STATIC_LIBS=TRUE`, `find_package(OpenSSL REQUIRED)`; `PQ_ARCH` defaults to `x86_64`; all exe targets get `target_link_options(-static)` and link `OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS}`.
- [ ] `armhf-toolchain.cmake`: `CMAKE_SYSTEM_NAME Linux`, processor `arm`, `CMAKE_C_COMPILER arm-linux-gnueabihf-gcc`, set `PQ_ARCH=armhf`, `CMAKE_FIND_ROOT_PATH` → armhf openssl prefix, find-root modes ONLY for libs/includes.
- [ ] Test `test_common`: `tcp_connect("pc-node",...)` returns -1 (non-numeric rejected); listen/connect/accept loopback on `127.0.0.1` round-trips a byte; `mono_ns()` monotonic increasing; `cpu_ns()` ≥ 0.
- [ ] **Verify:** `ctest` passes on x86_64.
- [ ] Commit.

### Task 3: `version` + `tlsctx` (libcommon part 2)

**Files:** Create `src/common/{version.c,version.h,tlsctx.c,tlsctx.h}`; extend `tests/test_common.c`.

**Interfaces produced:**
- `void assert_openssl_version(const char *want);` (abort+exit 3 if `OpenSSL_version(OPENSSL_VERSION)` lacks `want`).
- `SSL_CTX *make_ctx(int is_server, const char *profile, const char *ca, const char *cert, const char *key);` — sets min/max TLS1.3, cipher `TLS_AES_256_GCM_SHA384`, `SSL_CTX_set1_groups_list(exactly-one)`, `SSL_VERIFY_PEER|FAIL_IF_NO_PEER_CERT`, `SSL_OP_NO_TICKET`, `set_num_tickets(0)`. Returns NULL on any failure (e.g. group unknown).
- `const char *profile_group(const char *profile);` → `"X25519"`/`"X25519MLKEM768"`/NULL.

- [ ] Test: `assert_openssl_version("3.5.9")` passes; `make_ctx` with profile `pq` succeeds and with `profile_group("pq")` == `"X25519MLKEM768"`; `make_ctx` with a bogus group list returns NULL.
- [ ] **Verify:** `ctest` passes.
- [ ] Commit.

### Task 4: `kem_bench` (TOR §4 + §8 local)

**Files:** Create `src/kem_bench/main.c`; `tests/test_kem.c`.

**Interfaces consumed:** timing, csv, version.

- [ ] Implement: init provider/context **once** (timed separately, reported as `init_ns`); loop warmup+measured, each cycle: fresh `EVP_PKEY` keygen (ML-KEM-768), encapsulate, decapsulate, const-time compare secrets; time each op with `mono_ns`; write `cycle,keygen_ns,encaps_ns,decaps_ns,secret_match` to CSV. Print CPU via `cpu_ns`.
- [ ] Implement corrupt-ciphertext check: one success, then flip `ct[0]^=1`, decapsulate, assert secret differs (do **not** require decap error). Exit non-zero if secrets are equal.
- [ ] Test `test_kem`: a 3-cycle run returns all `secret_match=1`; the corrupt check yields differing secrets.
- [ ] **Verify:** `./kem_bench --warmup 2 --count 3 --csv /tmp/k.csv` exits 0; CSV has 3 rows.
- [ ] Commit.

### Task 5: `confirm` (exporter + HMAC) (libcommon part 3)

**Files:** Create `src/common/{confirm.c,confirm.h}`; `tests/test_confirm.c`.

**Interfaces produced:**
- `int export_app_key(SSL *s, const unsigned char *ctx32, unsigned char out32[32]);` (label fixed, use_context=1).
- `void confirm_tag(const unsigned char key32[32], const char *label, const unsigned char *id, size_t idlen, unsigned char out32[32]);`
- `int confirm_verify(const unsigned char key32[32], const char *label, const unsigned char *id, size_t idlen, const unsigned char tag32[32]);` (const-time; 1=ok).

- [ ] Test (no TLS needed for HMAC part): `confirm_tag` with known key/label/id is deterministic and differs across labels; `confirm_verify` accepts a matching tag and rejects a flipped-byte tag.
- [ ] **Verify:** `ctest` passes.
- [ ] Commit.

### Task 6: `pq_node` handshake core — `single` mode

**Files:** Create `src/pq_node/{main.c,args.c,args.h}`.

**Interfaces consumed:** net, tlsctx, confirm, timing, csv, version.

- [ ] Arg parse: `--role --profile --mode --peer --port --count --warmup --csv --expect-host --corrupt-confirm --drop-after-tcp`.
- [ ] Server `single`: accept 1 conn → `SSL_accept` → verify negotiated group==profile group (hard fail otherwise), version==TLSv1.3, cipher==`TLS_AES_256_GCM_SHA384`, `SSL_session_reused()==0` → recv 32-byte exp_id → `export_app_key` → send `server-confirm` tag, recv+verify `client-confirm` tag → exchange one fixed data record → print PASS line with non-secret params.
- [ ] Client `single`: `tcp_connect` → `SSL_connect` → same post-handshake checks → gen 32-byte exp_id (`RAND_bytes`), send it → `export_app_key` → send `client-confirm`, recv+verify `server-confirm` → data exchange → print PASS.
- [ ] **Verify (real two-process):** server `--role server --profile pq --mode single` + client `--role client --profile pq --mode single --expect-host pc-node` over loopback (certs from Task 8) → both print the negotiated group `X25519MLKEM768`, resumed=0, confirmation OK. Repeat `--profile baseline` → group `x25519`.
- [ ] Commit.

### Task 7: `pq_node` `bench-net` mode + measurement capture

**Files:** Modify `src/pq_node/main.c`.

- [ ] Server: accept `warmup+count` conns, do the full per-conn flow, capture per-conn on the server's own clock: `tls_handshake_ns`, `keyconfirm_ns`, `cpu_ns` (delta), `bytes_in/out` (`BIO_number_read/written` on the network BIO, read right after handshake = handshake bytes), `peak_rss_kb`, group/version/cipher/resumed/status; write `results/raw/net_<node>_<profile>.csv`.
- [ ] Client: same, plus `tcp_connect_ns`. Warm-up rows flagged/excluded by the analyzer.
- [ ] Negative hooks: `--corrupt-confirm` flips the client tag (expect server reject); `--drop-after-tcp` closes before TLS (expect counted error); server read timeout → counted timeout.
- [ ] **Verify:** a 2-warmup/5-count loopback run writes 7 rows each side; `status` all ok; resumed all 0.
- [ ] Commit.

### Task 8: Test CA + certs

**Files:** Create `scripts/make-certs.sh`.

- [ ] Using pinned `install/x86_64/openssl/bin/openssl`: ECDSA P-256 self-signed CA; `pc-node` and `opi-node` leaf certs (SAN `DNS:pc-node`+`IP:10.42.0.1`, resp. `DNS:opi-node`+`IP:10.42.0.2`), signed by CA; also a `rogue` self-signed cert for negative tests. Output public certs to `certs/`, keys to `certs/private/` (gitignored).
- [ ] **Verify:** `openssl verify -CAfile certs/ca.crt certs/pc-node.crt` → OK; `openssl verify` of rogue fails.
- [ ] Commit (public certs only; keys ignored).

### Task 9: Build+deploy+run scripts

**Files:** Create `scripts/build-all.sh`, `scripts/deploy-pi.sh`, `scripts/run-kem.sh`, `scripts/run-net.sh`, `scripts/run-failtests.sh`, `scripts/analyze.py`, `scripts/capture-env.sh`.

- [ ] `build-all.sh <arch>`: configure (x86_64 default, armhf via toolchain file) + build into `build/<arch>`.
- [ ] `deploy-pi.sh`: rsync armhf `kem_bench`,`pq_node`,`certs/` to the Pi `~/pqc/`.
- [ ] `capture-env.sh`: record CPU/OS/kernel/bits/compiler/openssl/net into `results/environment/<node>.txt`.
- [ ] `run-kem.sh`, `run-net.sh`: drive both nodes (server on PC via WSL, client on Pi via SSH), collect CSVs to `results/raw/`.
- [ ] `run-failtests.sh`: run the 7 negative scenarios, assert each fails/handles correctly, print a pass/fail table.
- [ ] `analyze.py`: read CSVs → median/p95/p99/max per metric/profile/node + success/error/timeout counts → `results/report/stats.md` + comparison table.
- [ ] **Verify:** scripts run with `--help`/dry sanity on the PC.
- [ ] Commit.

### Task 10: Full run on real hardware + report

**Files:** Create `docs/{README.md,BUILD.md,RUN.md,ENVIRONMENT.md,REPORT.md}`.

- [ ] Cross-build armhf, deploy to Pi; capture env on both nodes.
- [ ] KEM micro-test on PC and Pi (100+1000). Network runs both profiles (100+1000) with Pi=client, PC=server. Failure suite. Role-swap smoke (one short run PC=client/Pi=server) to prove it works.
- [ ] `analyze.py` → fill `REPORT.md`: environment table, KEM primitive timings (both arches), baseline-vs-PQ network comparison (median/p95/p99/max, bytes, RSS, exe size, counts), negative-test results, and the mandatory scope statements.
- [ ] **Verify:** completion criteria (§10 of spec) all met; raw CSVs present; report complete.
- [ ] Commit. (Push only if the user asks.)

---

## Self-Review notes
- Spec coverage: §2 env→Task 0/9; §3 OpenSSL/static→Task 1/2; §4 KEM→Task 4; §5 TLS→Task 3/6; §6 confirm→Task 5/6; §7 measure→Task 7/9; §8 failures→Task 4/7/9; §9 layout→all; §10 criteria→Task 10. No gaps.
- No placeholders: each task has concrete verify commands and interfaces.
- Type consistency: `export_app_key/confirm_tag/confirm_verify/make_ctx/profile_group` names reused verbatim across Tasks 3/5/6/7.
