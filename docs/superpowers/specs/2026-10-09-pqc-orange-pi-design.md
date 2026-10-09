# Design: Post-Quantum Session-Key Experiment (PC ↔ Orange Pi R1)

**Status:** Draft for review
**Date:** 2026-10-09
**Author:** experiment owner + Claude (Opus 4.8)
**Repo:** `D:\CPP\KEM` (new git repo, `main`)

---

## 1. Purpose

Verify that ML-KEM post-quantum key agreement works on a real ARM board and
measure the cost of integrating it, to inform a decision about moving the
solution into an Embedded-Linux product. The experiment produces reproducible
measurements of computation time, connection-establishment time, memory,
network volume, and error behaviour on **one specific** Orange Pi R1, plus a
baseline-vs-post-quantum comparison.

Two independent things are measured:

1. **Primitive cost (§4 of the TOR):** a local ML-KEM-768 micro-benchmark via the
   OpenSSL EVP API.
2. **Protocol cost (§5–§7 of the TOR):** full TLS 1.3 connections under two
   profiles that differ *only* in the key-agreement group.

**Scope boundary made explicit in the final report:** this experiment validates
the **post-quantum component of key agreement only**. Authentication uses
classical ECDSA P-256 certificates and stays classical. Results apply to this
Orange Pi R1 (Allwinner H2+, 32-bit ARMv7) and do **not** transfer to Cortex-M
microcontrollers.

---

## 2. Measured environment (pinned facts)

These are measured, not assumed. The build records them into
`results/environment/` at run time; the values below are the current readings.

### PC node (TLS server by default)
- Windows 11 Pro 26200 host running **WSL2 Ubuntu 26.04 LTS**.
- Kernel `6.6.87.2-microsoft-standard-WSL2`, `x86_64`, 64-bit userspace.
- gcc 15.2.0, glibc **2.43**, cmake (present in WSL; re-verified by setup).
- System OpenSSL 3.5.5 — **not used**; the experiment builds and links its own.
- **WSL2 networking: mirrored mode** (set in `%UserProfile%\.wslconfig`), so the
  Pi can reach the PC server directly over the LAN. The report states explicitly
  that the PC node runs under WSL2 and how networking was configured.

### Orange Pi node (TLS client by default)
- Board **Orange Pi R1**, SoC Allwinner **H2+** = quad **Cortex-A7**,
  **32-bit ARMv7 (`armv7l`, armhf)**, ~1 GHz, NEON/VFPv4 present. **Not AArch64.**
- **484 MiB RAM** + 242 MiB zram swap; 15 GB SD, ~12 GB free.
- **Armbian 26.8.0 trixie**, kernel `6.18.35-current-sunxi`, glibc **2.41**.
- System OpenSSL 3.5.6 — **not used**; the experiment ships its own statically.
- gcc 14.2.0 + make present; **no cmake, no git** (irrelevant — see build model).
- Headless (no video output).

### Link
- Direct Ethernet cable PC ↔ Pi, 100 Mbit/s negotiated, ~3 ms RTT.
- Bootstrap transport: IPv6 link-local SSH (`root@fe80::…%<ifindex>`, key auth).
- **Measurement transport: static IPv4 point-to-point** — PC `10.42.0.1/24`,
  Pi `10.42.0.2/24`. No DHCP, no switch, no router in the measurement path.
- Certificate identities use hostnames (`pc-node`, `opi-node`) mapped via
  `/etc/hosts` on both ends, so identity checks are independent of addressing.

### Network type note
The measured path is a direct Ethernet cable between the PC's physical NIC and
the Pi, with WSL2 in mirrored mode bridging the PC's server socket to that NIC.
The report records this as the network type and flags the WSL2 mirrored-mode
detail as a caveat.

---

## 3. Technology basis

- Language **C17**, build **CMake** for both architectures. The Pi needs no
  build tooling (no cmake/git): its armhf artifacts are cross-built on the PC
  and copied over.
- **OpenSSL pinned to 3.5.9** (3.5 LTS), built from source on both target
  architectures:
  - tarball `openssl-3.5.9.tar.gz`, released 2026-09-29,
  - **sha256 `603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`**,
  - verified by the build script before extraction.
- All crypto primitives and the hybrid group come from OpenSSL's built-in
  (default) provider. Randomness comes from OpenSSL's CSPRNG
  (`RAND_bytes`), seeded from the system entropy source.
- The application **proves** it uses the pinned library: it prints
  `OpenSSL_version(OPENSSL_VERSION)` at startup and asserts the runtime version
  string is `3.5.9`, exiting non-zero on mismatch. Because OpenSSL is linked
  **statically** into each binary, there is no possibility of silently loading
  the system `libssl`.

### Build & linking model (resolves the glibc gap)

WSL glibc (2.43) is newer than the Pi's (2.41), and glibc is **not** forward-
compatible, so a dynamically-linked armhf binary built on WSL would fail on the
Pi. Therefore:

- **Both** the x86_64 (PC) and armhf (Pi) binaries are **fully statically
  linked** (`-static`), embedding the pinned OpenSSL 3.5.9 (`libssl.a`,
  `libcrypto.a`) and glibc.
- armhf binaries are **cross-compiled on WSL** with `arm-linux-gnueabihf-gcc`
  (installed via apt; requires the user's sudo once) and then `rsync`'d to the
  Pi. The Pi needs no toolchain.
- Static glibc + networking is safe because the measurement path uses **numeric
  IPs** (`getaddrinfo` with `AI_NUMERICHOST`), so no NSS/DNS resolution occurs.
- OpenSSL is configured `no-shared no-dso no-tests` so the default provider
  (ML-KEM, X25519MLKEM768, AES-GCM, ECDSA, HMAC, SHA-3) is compiled in and
  fetched from the built-in provider with no runtime `.so` loading.

**Consequence for TOR wording:** the TOR asks to "verify the app loads the
needed library" and to report "executable size with dependencies." With static
linking: the loaded library is proven by the runtime version assertion and by
the fact that nothing else is linked; "size with dependencies" is reported as
the static binary size (all dependencies are inside it). This is a stronger
reproducibility guarantee than a dynamic `ldd` story and is documented as such.

---

## 4. Local ML-KEM-768 micro-test (`kem_bench`)

A standalone binary on each node. Uses the EVP API:

1. Generate a fresh ML-KEM-768 keypair (`EVP_PKEY_keygen` with the
   `ML-KEM-768` algorithm).
2. Encapsulate with the public key → `ciphertext` + `shared_secret_A`
   (`EVP_PKEY_encapsulate`).
3. Decapsulate with the private key → `shared_secret_B`
   (`EVP_PKEY_decapsulate`).
4. Assert `shared_secret_A == shared_secret_B` (constant-time compare).
5. Time **keygen, encapsulation, decapsulation separately.**

**Loop:** 100 warm-up + 1000 measured cycles. A **fresh keypair each cycle.**
Library init (`OSSL_PROVIDER` fetch, context creation) and helper-buffer
allocation are measured **separately** and reported once, outside the per-op
timers.

**Clocks:** `CLOCK_MONOTONIC` for wall-time per op; `CLOCK_PROCESS_CPUTIME_ID`
and `getrusage` for CPU. All timing is local; no cross-device arithmetic.

**Implicit-rejection test (TOR §8, local):** after a normal success, flip one
byte of a **correct-length** ciphertext, decapsulate, and assert the resulting
secret **differs** from the original. The test passes on "secrets differ" and
does **not** require `EVP_PKEY_decapsulate` to return an error (ML-KEM implicit
rejection may return a different secret with no API error).

**Output:** `results/raw/kem_<node>_<arch>.csv` — one row per measured cycle with
`cycle,keygen_ns,encaps_ns,decaps_ns,secret_match`.

---

## 5. TLS exchange: client & server (`pq_node`)

One binary, behaviour by flags:
`--role {server|client}`, `--profile {baseline|pq}`,
`--mode {bench-net|single}`, `--peer <ip>`, `--port <n>`,
`--count <n>`, `--warmup <n>`, `--csv <path>`.
Role is selectable so client/server can be swapped (TOR requirement).

| Parameter | Baseline profile | Post-quantum profile |
|---|---|---|
| Protocol | TLS 1.3 only | TLS 1.3 only |
| Key-agreement group | `X25519` | `X25519MLKEM768` |
| Cipher suite | `TLS_AES_256_GCM_SHA384` | `TLS_AES_256_GCM_SHA384` |
| Authentication | test ECDSA P-256 certs | same certs & settings |

**Group enforcement:** each profile calls
`SSL_CTX_set1_groups_list(ctx, "<group>")` with **exactly one** group and no
fallback. If the configured group is unavailable (e.g. PQ group missing), the
handshake fails and the binary exits non-zero with a clear message.

**Post-handshake verification & recording (every connection):**
- Negotiated group via `SSL_get0_group_name` / `SSL_get_negotiated_group`.
- TLS version via `SSL_get_version`; cipher via `SSL_get_cipher`.
- Resumption flag via `SSL_session_reused` (**must be 0**).
- A PQ run is accepted as post-quantum **only** if the negotiated group is
  literally `X25519MLKEM768`. TLS 1.3 + AES-256 alone are never treated as proof.

**Mutual authentication:** both sides require and verify the peer certificate
against the local test CA (`SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT`),
and check the expected peer identity (hostname/SAN via
`SSL_set1_host` / `X509_check_host`).

**Session resumption & 0-RTT disabled:** `SSL_OP_NO_TICKET`,
`SSL_CTX_set_num_tickets(ctx, 0)`, early-data off. Verified off per connection by
asserting `SSL_session_reused() == 0`.

---

## 6. Shared-key derivation & confirmation (TOR §6)

After a successful handshake, once per connection (both `single` and
`bench-net` modes):

1. **Client** generates a random **32-byte experiment ID** via `RAND_bytes` and
   sends it to the server inside the TLS channel.
2. Both sides call
   `SSL_export_keying_material(ssl, out, 32,
      "EXPERIMENTAL-pqc-orange-pi-v1", 29,
      exp_id, 32, /*use_context=*/1)`.
   The experiment ID is the **exporter context on both sides**.
3. Mutual confirmation via **HMAC-SHA-256** over the experiment ID keyed by the
   exported key, with distinct labels:
   - client sends `HMAC(K, "client-confirm" || exp_id)`,
   - server sends `HMAC(K, "server-confirm" || exp_id)`,
   - each side verifies the other's tag (constant-time); mismatch = failure.
4. After confirmation, a small application data exchange inside TLS
   (a fixed request/response) demonstrates the channel.

**Secrecy rules:** the exported key and the raw TLS secrets are **never** logged
or sent over the network. Only the confirmation **result** (pass/fail) and
non-secret connection parameters are recorded. The derived value is documented
as a **TLS-exporter application key**, explicitly **not** "the ML-KEM secret."

---

## 7. Measurements (TOR §7)

**Per network profile: 100 warm-up + 1000 measured full connections.** Between
the two profiles, certificate, application, and network parameters are held
identical; only the group changes.

**Timed separately, each on the local monotonic clock of the node that owns the
step** (never subtract timestamps across devices):
- TCP connect time,
- TLS handshake time,
- application-key confirmation time.

**Also measured:**
- CPU time per node (`CLOCK_PROCESS_CPUTIME_ID` deltas + `getrusage`).
- Handshake bytes **sent/received**, counted at the **TLS-record level** via
  `BIO_number_read/written` on the network BIO, labelled as such in the output.
- **Peak process RSS** (`getrusage.ru_maxrss`), reported as **whole-process**
  RSS — explicitly *not* presented as the memory of a single KEM operation.
- **Executable size with dependencies** = the static binary size.
- Counts of **successful** connections, **errors**, and **timeouts.**

**Raw data:** `results/raw/net_<node>_<profile>.csv`, one row per connection:
`conn,tcp_connect_ns,tls_handshake_ns,keyconfirm_ns,cpu_ns,bytes_in,bytes_out,
peak_rss_kb,group,tls_version,cipher,resumed,status`.

**Analysis:** `scripts/analyze.py` computes **median, p95, p99, max** per timing
metric per profile per node, plus success/error/timeout tallies, and emits a
comparison table. Failed attempts are counted explicitly and excluded from
timing percentiles (but reported). Energy is **not** estimated (no instrument).

---

## 8. Failure tests (TOR §8)

A scripted negative-test suite; overall success is recorded **only after all
pass**:

| Test | Expectation |
|---|---|
| Wrong/invalid peer certificate | handshake refused |
| Wrong peer identity (hostname/SAN mismatch) | handshake refused |
| No common group (client PQ-only vs server baseline-only) | handshake fails with clear error |
| Connection drop mid-handshake | detected, counted, no hang |
| Timeout (peer silent) | bounded wait, clean timeout, counted |
| Bad key confirmation (tampered tag) | confirmation fails |
| **Local:** ML-KEM ciphertext corruption | shared secrets differ (implicit rejection honoured; no API error required) |

---

## 9. Repository layout

```
KEM/
├─ CMakeLists.txt
├─ cmake/armhf-toolchain.cmake        # arm-linux-gnueabihf, -static
├─ .gitignore  .gitattributes         # keys excluded; LF enforced
├─ src/
│  ├─ common/   # timers, CSV, TLS ctx setup, exporter+HMAC confirm, version assert
│  ├─ kem_bench/
│  └─ pq_node/
├─ scripts/
│  ├─ build-openssl.sh <x86_64|armhf> # fetch+sha256+build into install/<arch>
│  ├─ build-all.sh <x86_64|armhf>     # configure+build the app for an arch
│  ├─ make-certs.sh                   # local CA + ECDSA P-256 node certs
│  ├─ setup-net-pc.ps1 / setup-net-pi.sh  # static 10.42.0.x, mirrored WSL
│  ├─ deploy-pi.sh                    # rsync static binaries + certs to Pi
│  ├─ run-kem.sh / run-net.sh         # drive micro-test / network runs
│  ├─ run-failtests.sh
│  └─ analyze.py
├─ 3rdparty/openssl/                  # (source tree + tarball gitignored)
├─ certs/                             # CA + public certs; *.key gitignored
├─ results/{environment,raw,report}/
└─ docs/{README,BUILD,RUN,ENVIRONMENT,REPORT}.md  (English)
```

Private keys never enter git (`.gitignore` + a pre-commit guard). The sibling
`VPN_Orange-Qalqan/` folder is left untouched and git-ignored.

---

## 10. Completion criteria (from the TOR)

1. The exchange works between the two **physical** devices.
2. The required group (`X25519MLKEM768`) is confirmed as actually negotiated.
3. The shared application key is successfully confirmed on both sides.
4. Negative tests correctly end in refusal.
5. Measurements are reproducible (pinned OpenSSL, fixed loop counts, raw CSVs,
   scripts, recorded environment).

**Deliverables:** sources, build + run instructions for both nodes, test
scripts, environment description, raw CSVs, and a short comparison report.

**Final-report statements (mandatory):** the experiment validates the
post-quantum part of key agreement; certificate authentication remains classical
(ECDSA); results are specific to this Orange Pi and do not transfer to Cortex-M.
PC/emulator numbers are never presented as Orange Pi results.

---

## 11. Risks & mitigations

- **Static glibc + name resolution:** mitigated by numeric-IP-only measurement
  path (`AI_NUMERICHOST`).
- **Cross-toolchain install needs sudo:** one-time `apt install
  gcc-arm-linux-gnueabihf`; the user runs it or grants sudo.
- **WSL2 mirrored mode + firewall:** inbound rule for the listen port; verified
  by a reachability check before any measurement run.
- **484 MiB RAM on the Pi at run time:** the app's own footprint is small; peak
  RSS is a measured output, not a build concern (build happens on the PC).
- **Pi reachable only via link-local until static IP is set:** bootstrap over
  IPv6 link-local SSH, then switch to `10.42.0.x`.
- **Clock domains:** every latency is computed within one device; cross-device
  subtraction is prohibited in code and review.
