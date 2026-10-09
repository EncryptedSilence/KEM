# Environment

The experiment records the exact environment of each node at run time into
[`results/environment/`](../results/environment/) via `scripts/capture-env.sh`.
This file summarises what was determined and how.

## Determined, not assumed (TOR §2)

The Orange Pi was **not** assumed to be AArch64. It was probed directly
(`uname -m`, `getconf LONG_BIT`, `/proc/cpuinfo`, `/etc/os-release`, `free`,
`openssl version`) and found to be 32-bit ARMv7.

| | PC node | Orange Pi node |
|---|---|---|
| CPU | x86-64 | Allwinner H2+, quad Cortex-A7 (ARMv7l), ~1 GHz, NEON/VFPv4 |
| Bits | 64 | **32 (armhf)** |
| RAM | host | 484 MiB + 242 MiB zram |
| OS | Ubuntu 26.04 under **WSL2** | Armbian 26.8.0 (Debian 13 trixie) |
| Kernel | 6.6.87.2-microsoft-standard-WSL2 | 6.18.35-current-sunxi |
| Compiler | gcc 15.2.0 | cross: Bootlin arm-buildroot-linux-gnueabihf gcc 15.3.0 |
| System OpenSSL (unused) | 3.5.5 | 3.5.6 |
| Linked OpenSSL | **3.5.9** (static, pinned) | **3.5.9** (static, pinned) |
| glibc | 2.43 | 2.41 |
| Network | direct Ethernet, 100 Mbit/s, ~3–4 ms RTT | same |

## WSL2

The PC node runs under **WSL2** (Windows 11 host). Networking mode is
**mirrored** (`%UserProfile%\.wslconfig` → `[wsl2] networkingMode=mirrored`) so
WSL shares the Windows Ethernet adapter and can reach the Pi directly on the
point-to-point link. Inbound TCP to a WSL-bound port is not accepted on this
host (a documented WSL mirrored-mode limitation), which is why the measured
configuration uses the Pi as the TLS server — see [`REPORT.md`](REPORT.md) §8.

## OpenSSL pinning & linking

OpenSSL 3.5.9 (3.5 LTS) is built from source, sha256-verified, and **statically
linked** into every binary (`no-shared no-dso`). The binaries therefore do not
depend on either node's system OpenSSL (3.5.5 / 3.5.6) and are immune to the
PC↔Pi glibc version gap. Each binary asserts at startup that its linked OpenSSL
version string contains `3.5.9` and exits with code 3 otherwise, so the report's
"which library was used" claim is machine-checked, not assumed.

## Addressing

Static point-to-point IPv4 over the direct Ethernet cable: PC `10.42.0.1/24`,
Pi `10.42.0.2/24`. Certificate identities are hostnames (`pc-node`, `opi-node`)
verified against the certificate SANs independently of the numeric connect
address. The data path uses numeric IPs only (no DNS/NSS), which keeps the
static binaries free of name-resolution dependencies.
