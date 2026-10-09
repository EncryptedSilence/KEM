#!/usr/bin/env bash
# Negative-test suite (TOR 8). Runs on x86_64 loopback — every case here is a
# protocol/behaviour property independent of arch or link. Each test asserts
# the EXPECTED failure/handling. Overall success only if all pass.
set -uo pipefail
ARCH="${1:-x86_64}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$REPO/build/$ARCH/pq_node"
KEM="$REPO/build/$ARCH/kem_bench"
C="$REPO/certs"
PORT=45500
pass=0; fail=0
IP=127.0.0.1

# Launch the server in the FOREGROUND of a backgrounded subshell so the main
# shell can wait on it and read its true exit code.
server() { # profile cert key expect-host [extra args...]
  local pr="$1" cert="$2" key="$3" eh="$4"; shift 4
  "$BIN" --role server --profile "$pr" --mode single --bind $IP --port $PORT \
    --ca "$C/ca.crt" --cert "$cert" --key "$key" --expect-host "$eh" --timeout-ms 1500 "$@" \
    >/tmp/ft_srv.out 2>&1
}
client() { # profile cert key expect-host [extra args...]
  local pr="$1" cert="$2" key="$3" eh="$4"; shift 4
  "$BIN" --role client --profile "$pr" --mode single --peer $IP --port $PORT \
    --ca "$C/ca.crt" --cert "$cert" --key "$key" --expect-host "$eh" --timeout-ms 1500 "$@" \
    >/tmp/ft_cli.out 2>&1
}

# Run one scenario; returns via globals SRC (server rc) and CRC (client rc).
run_pair() { # srv(4+args separated by '||') ... we pass arrays via positional
  :
}

record_reject() { # name  srv_rc  cli_rc   (pass if either failed)
  local name="$1" src="$2" crc="$3"
  if [ "$src" != 0 ] || [ "$crc" != 0 ]; then
    echo "  PASS  $name (rejected: srv=$src cli=$crc)"; pass=$((pass+1))
  else
    echo "  FAIL  $name (expected failure, both succeeded)"; fail=$((fail+1))
  fi
}
record_server_fail() { # name  srv_rc  (pass if server failed/handled, not hung)
  local name="$1" src="$2"
  if [ "$src" != 0 ]; then echo "  PASS  $name (server handled: srv=$src)"; pass=$((pass+1))
  else echo "  FAIL  $name (server did not fail)"; fail=$((fail+1)); fi
}

echo "== negative tests =="

# 1. wrong/invalid client certificate (rogue, not signed by CA)
server pq "$C/pc-node.crt" "$C/private/pc-node.key" pc-node & S=$!
sleep 0.3
client pq "$C/rogue.crt" "$C/private/rogue.key" pc-node; CRC=$?
wait $S; SRC=$?
record_reject "wrong-certificate" "$SRC" "$CRC"; PORT=$((PORT+1))

# 2. wrong peer identity (valid cert, server expects a name not in client SAN)
server pq "$C/pc-node.crt" "$C/private/pc-node.key" no-such-host & S=$!
sleep 0.3
client pq "$C/opi-node.crt" "$C/private/opi-node.key" pc-node; CRC=$?
wait $S; SRC=$?
record_reject "wrong-identity" "$SRC" "$CRC"; PORT=$((PORT+1))

# 3. no common group (server baseline-only vs client pq-only)
server baseline "$C/pc-node.crt" "$C/private/pc-node.key" opi-node & S=$!
sleep 0.3
client pq "$C/opi-node.crt" "$C/private/opi-node.key" pc-node; CRC=$?
wait $S; SRC=$?
record_reject "no-common-group" "$SRC" "$CRC"; PORT=$((PORT+1))

# 4. connection dropped before TLS (client exits 0 by design; server must fail)
server pq "$C/pc-node.crt" "$C/private/pc-node.key" opi-node & S=$!
sleep 0.3
client pq "$C/opi-node.crt" "$C/private/opi-node.key" pc-node --drop-after-tcp; CRC=$?
wait $S; SRC=$?
record_server_fail "connection-drop" "$SRC"; PORT=$((PORT+1))

# 5. timeout (client connects, stalls past server timeout, never speaks TLS)
server pq "$C/pc-node.crt" "$C/private/pc-node.key" opi-node & S=$!
sleep 0.3
client pq "$C/opi-node.crt" "$C/private/opi-node.key" pc-node --stall-ms 3000; CRC=$?
wait $S; SRC=$?
record_server_fail "timeout" "$SRC"; PORT=$((PORT+1))

# 6. bad key confirmation (client tampers its tag)
server pq "$C/pc-node.crt" "$C/private/pc-node.key" opi-node & S=$!
sleep 0.3
client pq "$C/opi-node.crt" "$C/private/opi-node.key" pc-node --corrupt-confirm; CRC=$?
wait $S; SRC=$?
record_reject "bad-key-confirm" "$SRC" "$CRC"; PORT=$((PORT+1))

# 7. local ML-KEM ciphertext corruption (implicit rejection)
if "$KEM" --warmup 1 --count 1 2>/dev/null | grep -q "implicit_rejection_differs=1"; then
  echo "  PASS  kem-ciphertext-corruption (secret differs, implicit rejection)"; pass=$((pass+1))
else
  echo "  FAIL  kem-ciphertext-corruption"; fail=$((fail+1))
fi

echo "== negative tests: pass=$pass fail=$fail =="
[ "$fail" = 0 ] && echo "FAILTESTS_OK" || echo "FAILTESTS_FAIL"
[ "$fail" = 0 ]
