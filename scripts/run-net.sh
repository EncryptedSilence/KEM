#!/usr/bin/env bash
# Full-connection network measurements for both profiles.
#
# Direction: Orange Pi = TLS server, PC (WSL) = TLS client. Roles are swapped
# from the TOR's nominal "Pi=client" because WSL2 mirrored networking does not
# accept inbound TCP to a WSL-bound port on this host (even from the Windows
# host itself); WSL *outbound* to the Pi works and the Pi serves fine. The code
# supports both roles; the baseline-vs-PQ comparison and the per-node
# CPU/handshake metrics are unaffected by the direction.
#
#   PI_HOST (default 10.42.0.2), PORT, WARMUP, COUNT.
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/pi-ssh.sh"

PI_HOST="${PI_HOST:-10.42.0.2}"
PORT="${PORT:-44330}"
WARMUP="${WARMUP:-100}"; COUNT="${COUNT:-1000}"
C="$REPO/certs"
BIN="$REPO/build/x86_64/pq_node"
mkdir -p "$REPO/results/raw"

run_profile() {
  local profile="$1"
  echo "=== profile=$profile (server=Pi $PI_HOST, client=PC/WSL) ==="
  local ccsv="$REPO/results/raw/net_pc_${profile}.csv"     # PC = client
  local scsv_pi="results/raw/net_opi_${profile}.csv"       # Pi = server (rel ~/pqc)

  # Pi server runs in the FOREGROUND of a backgrounded ssh: the ssh channel
  # keeps it alive for the whole bench; it exits after warmup+count conns.
  ssh $PI_SSH_OPTS "$PI_USER@$PI_HOST" \
      "cd ~/pqc && pkill -x pq_node 2>/dev/null; sleep 0.3; \
       ./pq_node --role server --profile $profile --mode bench-net --bind $PI_HOST --port $PORT \
       --warmup $WARMUP --count $COUNT --csv $scsv_pi \
       --ca certs/ca.crt --cert certs/opi-node.crt --key certs/private/opi-node.key \
       --expect-host pc-node >/tmp/pisrv_${profile}.out 2>&1" &
  local SRV=$!

  # Wait until the Pi is actually listening before the client starts (avoids a
  # startup race where early connects get refused).
  local ready=0
  for _i in $(seq 1 30); do
    if pi_ssh "ss -ltn 2>/dev/null | grep -q :$PORT" 2>/dev/null; then ready=1; break; fi
    sleep 0.5
  done
  [ "$ready" = 1 ] || echo "WARNING: Pi server not listening after wait"

  "$BIN" --role client --profile "$profile" --mode bench-net --peer "$PI_HOST" --port "$PORT" \
      --warmup "$WARMUP" --count "$COUNT" --csv "$ccsv" \
      --ca "$C/ca.crt" --cert "$C/pc-node.crt" --key "$C/private/pc-node.key" \
      --expect-host opi-node >"$REPO/results/raw/net_pc_${profile}.log" 2>&1 || true

  wait "$SRV" 2>/dev/null || true
  pi_ssh "cat ~/pqc/$scsv_pi" > "$REPO/results/raw/net_opi_${profile}.csv" 2>/dev/null || true
  pi_ssh "grep BENCH_SUMMARY /tmp/pisrv_${profile}.out" > "$REPO/results/raw/net_opi_${profile}.log" 2>/dev/null || true
  grep -h BENCH_SUMMARY "$REPO/results/raw/net_pc_${profile}.log" "$REPO/results/raw/net_opi_${profile}.log" 2>/dev/null || true
}

run_profile baseline
run_profile pq
echo ">> network runs collected into results/raw/"
