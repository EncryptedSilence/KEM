# Run instructions

Prereqs: everything in [`BUILD.md`](BUILD.md) is built, and you can reach the
Orange Pi over SSH. The scripts assume key-based SSH as `root`; set `PI_HOST`
to the Pi's address.

## 1. Network link (static point-to-point Ethernet)

Direct Ethernet cable PC ↔ Pi. Static IPs, no DHCP/switch.

**PC (Windows, elevated PowerShell):**
```powershell
# static IP on the Ethernet adapter + inbound firewall rule
powershell -ExecutionPolicy Bypass -File scripts\setup-net-pc.ps1
# WSL2 mirrored networking (restarts WSL)
powershell -ExecutionPolicy Bypass -File scripts\setup-wsl-mirrored.ps1
```

**Orange Pi (over SSH):**
```bash
ssh root@<pi-link-local-or-current-addr> 'bash -s' < scripts/setup-net-pi.sh
# assigns 10.42.0.2/24 to the cabled interface (detected by MAC)
```

Verify from WSL: `ping -c3 10.42.0.2` → 0% loss.

> If the Pi starts only reachable over IPv6 link-local on the cable
> (`fe80::…%<ifindex>`), use that address for the first SSH to run
> `setup-net-pi.sh`, then switch to `10.42.0.2`.

### WSL2 mirrored-mode inbound (only if PC must be the server)

On this host, inbound TCP to a WSL-bound port is blocked even from Windows
itself, so the measurements run with **Pi = server, PC = client** (see
`REPORT.md` §8). If you want the PC to be the server, allow inbound to WSL via
the Hyper-V firewall (elevated):
```powershell
powershell -ExecutionPolicy Bypass -File scripts\fix-wsl-firewall.ps1 -Port 44330
```

## 2. Deploy to the Pi

```bash
PI_HOST=10.42.0.2 bash scripts/deploy-pi.sh      # rsync armhf binaries + certs to ~/pqc
```

## 3. Capture environments

```bash
bash scripts/capture-env.sh pc x86_64                         # PC -> results/environment/pc.txt
ssh root@10.42.0.2 'cd ~/pqc && bash capture-env.sh opi armhf'
ssh root@10.42.0.2 'cat ~/results/environment/opi.txt' > results/environment/opi.txt
```

## 4. Measurements (100 warm-up + 1000 measured)

```bash
# ML-KEM micro-benchmark, both nodes
PI_HOST=10.42.0.2 bash scripts/run-kem.sh
# or run per node:
build/x86_64/kem_bench --warmup 100 --count 1000 --csv results/raw/kem_pc_x86_64.csv
ssh root@10.42.0.2 'cd ~/pqc && ./kem_bench --warmup 100 --count 1000 --csv /root/kem_opi_armhf.csv'

# Full TLS connections, both profiles (Pi=server, PC=client)
PI_HOST=10.42.0.2 WARMUP=100 COUNT=1000 bash scripts/run-net.sh

# Negative tests
bash scripts/run-failtests.sh x86_64
```

Raw CSVs land in `results/raw/`. The Pi's CSVs are pulled back automatically.

## 5. Analyse

```bash
python3 scripts/analyze.py --out results/report/stats.md results/raw/*.csv
```

Produces median/p95/p99/max per metric per profile per node, byte counts, peak
RSS and success/error/timeout tallies. See [`REPORT.md`](REPORT.md) for the
written comparison.

## Manual single handshake (sanity)

```bash
# Pi server:
ssh root@10.42.0.2 'cd ~/pqc && ./pq_node --role server --profile pq --mode single \
  --bind 10.42.0.2 --port 44330 --ca certs/ca.crt --cert certs/opi-node.crt \
  --key certs/private/opi-node.key --expect-host pc-node'
# PC client:
build/x86_64/pq_node --role client --profile pq --mode single --peer 10.42.0.2 --port 44330 \
  --ca certs/ca.crt --cert certs/pc-node.crt --key certs/private/pc-node.key --expect-host opi-node
```

Both print the negotiated group (`X25519MLKEM768`), TLS version, cipher,
`resumed=0`, and confirm the shared application key.
