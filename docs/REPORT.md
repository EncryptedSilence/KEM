# Post-Quantum Session-Key Experiment — Report

**PC (WSL2) ↔ Orange Pi R1, ML-KEM-768 / X25519MLKEM768 over TLS 1.3, OpenSSL 3.5.9**
Date: 2026-10-09. Raw data: [`results/raw/`](../results/raw/); per-metric statistics: [`results/report/stats.md`](../results/report/stats.md); environments: [`results/environment/`](../results/environment/).

## 1. Conclusion

ML-KEM works on this real 32-bit ARM board and the cost of adding the
post-quantum hybrid group to a TLS 1.3 session is **modest**:

- **Handshake latency:** +~1.2 ms median (baseline `X25519` → PQ `X25519MLKEM768`), on both the PC-client and Pi-server views.
- **Network volume:** +~1.1 KB in each direction at the TLS-record level (the ML-KEM-768 public key + ciphertext).
- **Pi CPU per connection:** +~1.2 ms (≈ +8 %).
- **Memory:** no material difference in peak process RSS between profiles.
- **Reliability:** 1000/1000 measured connections succeeded for **both** profiles; all negative tests fail correctly.

The post-quantum primitive itself on the Cortex-A7 costs sub-millisecond per
operation (keygen ~0.70 ms, encapsulation ~0.40 ms, decapsulation ~0.62 ms),
about **30× slower than the x86-64 PC** but still small relative to the network
round-trips of a handshake.

**Decision input:** integrating ML-KEM hybrid key agreement on an Orange Pi
R1-class device is cheap in time, memory and bandwidth. The dominant handshake
cost is network round-trips, not the post-quantum math.

## 2. Scope and honesty statements (required)

- This experiment validates the **post-quantum component of key agreement**
  (the `X25519MLKEM768` hybrid group). It was confirmed as the *actually
  negotiated* group on every connection — not inferred from "TLS 1.3 + AES-256".
- **Authentication remains classical**: mutual X.509 with **ECDSA P-256**
  certificates from a local test CA. Only key agreement is post-quantum.
- The application confirmation key is a **TLS exporter-derived key**
  (`SSL_export_keying_material`), **not** the ML-KEM shared secret itself.
- Results are specific to **this** Orange Pi R1 (Allwinner H2+, ARMv7) and **do
  not transfer** to Cortex-M microcontrollers.
- No PC or emulator figure is presented as an Orange Pi figure; every number is
  labelled by the node that produced it, measured on that node's own monotonic
  clock. No latency is computed by subtracting clocks of different devices.
- Energy consumption was **not** estimated (no instrumentation).

## 3. Environment (pinned)

| | PC node | Orange Pi node |
|---|---|---|
| Role in measurement | TLS **client** | TLS **server** (see §7) |
| Hardware | x86-64 host | Orange Pi R1, Allwinner **H2+**, quad **Cortex-A7**, ~1 GHz |
| Arch / bits | x86_64, 64-bit | **armv7l, 32-bit (armhf)** — NEON/VFPv4 |
| RAM | (host) | **484 MiB** + 242 MiB zram |
| OS / kernel | **WSL2 Ubuntu 26.04**, 6.6.87.2-microsoft-standard-WSL2 | **Armbian 26.8.0 trixie**, 6.18.35-current-sunxi |
| libc | glibc 2.43 | glibc 2.41 |
| Compiler | gcc 15.2.0 | cross: Bootlin `arm-buildroot-linux-gnueabihf` gcc 15.3.0 |
| OpenSSL (linked) | **3.5.9** (static, pinned) | **3.5.9** (static, pinned) |
| OpenSSL (system, unused) | 3.5.5 | 3.5.6 |
| Network | direct Ethernet cable, 100 Mbit/s, ~3–4 ms RTT, static `10.42.0.1` ↔ `10.42.0.2` | same link |

Full captures: [`results/environment/pc.txt`](../results/environment/pc.txt), [`results/environment/opi.txt`](../results/environment/opi.txt).

**WSL2 note:** the PC node runs under WSL2 in **mirrored** networking mode. The
pinned OpenSSL 3.5.9 is **statically linked** into every binary; the Pi's
*system* OpenSSL is 3.5.6, but our binaries embed 3.5.9 and assert it at
startup (`OpenSSL_version` must contain `3.5.9`, else exit 3). Static linking
makes "which library is loaded" provable and "size with dependencies" equal to
the binary size.

## 4. Local ML-KEM-768 micro-benchmark (TOR §4)

100 warm-up + 1000 measured cycles, a **fresh keypair each cycle**, each
operation timed separately on `CLOCK_MONOTONIC`. Library init and buffer
allocation are excluded from the operation timers. Median (ms):

| operation | PC (x86-64) | Orange Pi (ARMv7) | Pi / PC |
|---|---|---|---|
| key generation | 0.023 | 0.700 | ~30× |
| encapsulation | 0.013 | 0.404 | ~31× |
| decapsulation | 0.020 | 0.621 | ~31× |

p95/p99/max in [`results/report/stats.md`](../results/report/stats.md). Shared secrets matched in
**all 1000 cycles** on both nodes (0 mismatches). The library-init cost
(first-use provider load, reported separately) was ~14–29 ms once per process.

**Implicit rejection (TOR §8, local):** flipping one byte of a correct-length
ML-KEM ciphertext yields a **different** shared secret with **no** API error —
verified on both nodes (`implicit_rejection_differs=1`). The test asserts the
secrets differ, as required; it does not depend on a decapsulation error.

## 5. Network comparison: baseline vs post-quantum (TOR §5–§7)

100 warm-up + 1000 measured full connections per profile. Session resumption and
0-RTT disabled and verified off (`SSL_session_reused()==0` on every connection).
Certificates, application and link identical between profiles; only the group
differs. Medians:

| metric (median) | Baseline `X25519` | PQ `X25519MLKEM768` | Δ |
|---|---|---|---|
| TLS handshake — PC client | 8.705 ms | 9.944 ms | **+1.24 ms** |
| TLS handshake — Pi server | 14.624 ms | 15.860 ms | **+1.24 ms** |
| Pi server CPU / connection | 14.203 ms | 15.394 ms | **+1.19 ms** |
| PC client CPU / connection | 0.714 ms | 0.792 ms | +0.08 ms |
| TCP connect (PC client) | 1.377 ms | 1.381 ms | ~0 (link) |
| Handshake bytes in / out | 1283 / 1260 | 2371 / 2436 | **+~1.1 KB each way** |
| Peak process RSS (Pi) | 5080 KB | 5080 KB | ~0 |

(`tcp_connect` on the server is 0 by construction — the server accepts, it does
not connect. Percentiles p95/p99/max are in `stats.md`.)

**Negotiated parameters (recorded every connection):** version `TLSv1.3`,
cipher `TLS_AES_256_GCM_SHA384`, group exactly `x25519` (baseline) or
`X25519MLKEM768` (PQ), `resumed=0`. A PQ run is accepted only when the
negotiated group is literally `X25519MLKEM768`.

**Reading the key-confirmation time.** The PC-client `key_confirm` (~9.2 ms) is
dominated by **two network round-trips** (exchange experiment-id + tags, then a
PING/PONG) over the ~4 ms-RTT link; the exporter + HMAC-SHA-256 compute itself
is microseconds. It is essentially identical across profiles (the exporter does
not depend on the KEM), so it is not a post-quantum cost. The Pi-server
`key_confirm` (~1.9 ms) reflects its local half of that exchange.

**Counts:** 1000/1000 measured connections succeeded for both profiles on both
nodes; 0 errors, 0 timeouts. Raw per-connection CSVs: `results/raw/net_*_*.csv`.

## 6. Shared-key derivation & confirmation (TOR §6)

After each handshake the client generates a random 32-byte experiment id and
sends it inside TLS; both sides call `SSL_export_keying_material` for 32 bytes
with label `EXPERIMENTAL-pqc-orange-pi-v1` and the experiment id as exporter
context (`use_context=1`); they then confirm agreement by exchanging
HMAC-SHA-256 tags over the id with labels `client-confirm` / `server-confirm`.
The exported key and TLS secrets are never logged or sent. Confirmation
succeeded on every successful connection; a tampered tag is rejected (§7 tests).

## 7. Failure tests (TOR §8)

All pass (suite `scripts/run-failtests.sh`, on the PC over loopback — each case
is a protocol/behaviour property independent of arch or link):

| test | result |
|---|---|
| wrong / untrusted certificate (rogue, not CA-signed) | handshake refused ✓ |
| wrong peer identity (SAN mismatch) | handshake refused ✓ |
| no common group (baseline-only vs PQ-only) | handshake fails with clear error ✓ |
| connection dropped before TLS | server detects & counts, no hang ✓ |
| peer stalls / timeout | server times out cleanly ✓ |
| bad key confirmation (tampered tag) | confirmation rejected ✓ |
| ML-KEM ciphertext corruption (implicit rejection) | secret differs, no error ✓ (both nodes) |

## 8. Methodology notes & caveats

- **Roles are swapped vs the TOR's nominal "Pi = client".** WSL2 mirrored
  networking on this host does not accept inbound TCP to a WSL-bound port (even
  the Windows host cannot connect to it), so the PC cannot be the reachable
  server. WSL *outbound* works and the Pi serves cleanly, so the measured
  configuration is **Pi = server, PC = client**. The code supports either role
  via `--role` (both directions shown on loopback); the baseline-vs-PQ
  comparison and per-node CPU/latency are unaffected by the direction. This is
  called out so the client/server labels in the tables are not misread.
- **Bytes accounting** is at the **TLS-record level** via OpenSSL BIO byte
  counters, read immediately after the handshake — not wire/TCP bytes.
- **Peak RSS** is whole-process (`getrusage`), never "one KEM's memory".
- **Executable size with dependencies** (static, all deps embedded): Pi
  `pq_node` ≈ 5.94 MB, `kem_bench` ≈ 4.96 MB; PC `pq_node` ≈ 8.1 MB.
- All timing uses local monotonic clocks; no cross-device timestamp subtraction.

## 9. Reproduction

See [`BUILD.md`](BUILD.md) and [`RUN.md`](RUN.md). In short: build pinned
OpenSSL 3.5.9 (sha256-verified) and the app for each arch, issue test certs,
set the static link, then run `scripts/run-kem.sh`, `scripts/run-net.sh`,
`scripts/run-failtests.sh`, and `scripts/analyze.py`.
