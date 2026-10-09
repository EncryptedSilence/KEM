#!/usr/bin/env bash
# Run the ML-KEM-768 micro-benchmark on both nodes (100 warmup + 1000 measured)
# and collect CSVs into results/raw/. Run from WSL (orchestrates PC + Pi).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/pi-ssh.sh"
WARMUP="${WARMUP:-100}"; COUNT="${COUNT:-1000}"
mkdir -p "$REPO/results/raw"

echo "=== PC (x86_64) KEM ==="
"$REPO/build/x86_64/kem_bench" --warmup "$WARMUP" --count "$COUNT" \
    --label pc-x86_64 --csv "$REPO/results/raw/kem_pc_x86_64.csv" | tee "$REPO/results/raw/kem_pc_x86_64.log"

echo "=== Orange Pi (armhf) KEM ==="
pi_ssh "cd ~/pqc && ./kem_bench --warmup $WARMUP --count $COUNT --label opi-armhf --csv results/raw/kem_opi_armhf.csv" \
    | tee "$REPO/results/raw/kem_opi_armhf.log"
rsync -az -e "ssh $PI_SSH_OPTS" "$PI_USER@$PI_HOST:pqc/results/raw/kem_opi_armhf.csv" "$REPO/results/raw/"
echo ">> KEM runs collected"
