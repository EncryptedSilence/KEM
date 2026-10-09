#!/usr/bin/env bash
# Loopback single-handshake smoke test for one profile.
# Usage: smoke-single.sh <x86_64|armhf-ignored> <baseline|pq> [port]
set -uo pipefail
ARCH="${1:-x86_64}"
PROFILE="${2:-pq}"
PORT="${3:-44330}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$REPO/build/$ARCH/pq_node"
C="$REPO/certs"

"$BIN" --role server --profile "$PROFILE" --mode single --bind 127.0.0.1 --port "$PORT" \
    --ca "$C/ca.crt" --cert "$C/pc-node.crt" --key "$C/private/pc-node.key" \
    --expect-host opi-node >/tmp/srv.out 2>&1 &
SRV=$!
sleep 0.3
"$BIN" --role client --profile "$PROFILE" --mode single --peer 127.0.0.1 --port "$PORT" \
    --ca "$C/ca.crt" --cert "$C/opi-node.crt" --key "$C/private/opi-node.key" \
    --expect-host pc-node >/tmp/cli.out 2>&1
CRC=$?
wait $SRV; SRC=$?
echo "=== server (rc=$SRC) ==="; cat /tmp/srv.out
echo "=== client (rc=$CRC) ==="; cat /tmp/cli.out
[ "$SRC" = 0 ] && [ "$CRC" = 0 ] && echo "SMOKE_OK $PROFILE" || echo "SMOKE_FAIL $PROFILE"
