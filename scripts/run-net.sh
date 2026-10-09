#!/usr/bin/env bash
# Run full-connection network measurements for both profiles between PC (server)
# and Orange Pi (client). 100 warmup + 1000 measured per profile. Collects a
# CSV from each node per profile. Run from WSL.
#   PC_IP   server bind/connect address (default 10.42.0.1)
#   WARMUP/COUNT override loop sizes.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/pi-ssh.sh"

PC_IP="${PC_IP:-10.42.0.1}"
PORT="${PORT:-44330}"
WARMUP="${WARMUP:-100}"; COUNT="${COUNT:-1000}"
C="$REPO/certs"
BIN="$REPO/build/x86_64/pq_node"
mkdir -p "$REPO/results/raw"

run_profile() {
  local profile="$1"
  echo "=== profile=$profile (server=PC $PC_IP, client=Pi) ==="
  local scsv="$REPO/results/raw/net_pc_${profile}.csv"
  local ccsv_pi="pqc/results/raw/net_opi_${profile}.csv"

  # server on the PC (WSL), listening on the point-to-point IP
  "$BIN" --role server --profile "$profile" --mode bench-net --bind "$PC_IP" --port "$PORT" \
      --warmup "$WARMUP" --count "$COUNT" --csv "$scsv" \
      --ca "$C/ca.crt" --cert "$C/pc-node.crt" --key "$C/private/pc-node.key" \
      --expect-host opi-node >"$REPO/results/raw/net_pc_${profile}.log" 2>&1 &
  local srv=$!
  sleep 0.5

  # client on the Pi
  pi_ssh "cd ~/pqc && ./pq_node --role client --profile $profile --mode bench-net \
      --peer $PC_IP --port $PORT --warmup $WARMUP --count $COUNT --csv $ccsv_pi \
      --ca certs/ca.crt --cert certs/opi-node.crt --key certs/private/opi-node.key \
      --expect-host pc-node" >"$REPO/results/raw/net_opi_${profile}.log" 2>&1 || true
  wait "$srv" || true

  rsync -az -e "ssh $PI_SSH_OPTS" "$PI_USER@$PI_HOST:$ccsv_pi" "$REPO/results/raw/" || true
  grep -h BENCH_SUMMARY "$REPO/results/raw/net_pc_${profile}.log" "$REPO/results/raw/net_opi_${profile}.log" || true
}

run_profile baseline
run_profile pq
echo ">> network runs collected into results/raw/"
