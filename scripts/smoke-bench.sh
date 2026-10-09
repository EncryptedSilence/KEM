#!/usr/bin/env bash
# Loopback bench-net smoke: small warmup+count, check row counts and status.
# Usage: smoke-bench.sh <arch> <profile> <warmup> <count> [port]
set -uo pipefail
ARCH="${1:-x86_64}"; PROFILE="${2:-pq}"; W="${3:-2}"; N="${4:-5}"; PORT="${5:-44340}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$REPO/build/$ARCH/pq_node"; C="$REPO/certs"
mkdir -p "$REPO/results/raw"
SCSV="$REPO/results/raw/smoke_server_${PROFILE}.csv"
CCSV="$REPO/results/raw/smoke_client_${PROFILE}.csv"

"$BIN" --role server --profile "$PROFILE" --mode bench-net --bind 127.0.0.1 --port "$PORT" \
    --warmup "$W" --count "$N" --csv "$SCSV" \
    --ca "$C/ca.crt" --cert "$C/pc-node.crt" --key "$C/private/pc-node.key" \
    --expect-host opi-node >/tmp/bsrv.out 2>&1 &
SRV=$!
sleep 0.3
"$BIN" --role client --profile "$PROFILE" --mode bench-net --peer 127.0.0.1 --port "$PORT" \
    --warmup "$W" --count "$N" --csv "$CCSV" \
    --ca "$C/ca.crt" --cert "$C/opi-node.crt" --key "$C/private/opi-node.key" \
    --expect-host pc-node >/tmp/bcli.out 2>&1
CRC=$?
wait $SRV; SRC=$?
echo "=== server rc=$SRC ==="; grep BENCH_SUMMARY /tmp/bsrv.out || cat /tmp/bsrv.out
echo "=== client rc=$CRC ==="; grep BENCH_SUMMARY /tmp/bcli.out || cat /tmp/bcli.out
exp=$((W+N))
srows=$(($(wc -l < "$SCSV")-1)); crows=$(($(wc -l < "$CCSV")-1))
echo "server_rows=$srows client_rows=$crows expected=$exp"
okc=$(grep -c ',ok$' "$CCSV"); res1=$(awk -F, 'NR>1 && $13==0' "$CCSV" | wc -l)
echo "client_ok_rows=$okc resumed0_rows=$res1"
if [ "$SRC" = 0 ] && [ "$CRC" = 0 ] && [ "$srows" = "$exp" ] && [ "$crows" = "$exp" ] && [ "$okc" = "$exp" ]; then
  echo "BENCH_SMOKE_OK $PROFILE"
else echo "BENCH_SMOKE_FAIL $PROFILE"; fi
