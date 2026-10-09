#!/usr/bin/env bash
# Assign the static point-to-point IPv4 on the Orange Pi's cabled interface.
# Run ON the Pi (e.g. piped over SSH). Non-persistent (runtime only), by design
# — it must not disturb the sibling VPN project's netplan. Re-run after reboot.
set -uo pipefail
MAC="${PI_CABLE_MAC:-c0:74:2b:ff:95:f8}"
ADDR="${PI_ADDR:-10.42.0.2/24}"

IFACE="$(ip -o link show | awk -v m="$MAC" 'tolower($0) ~ m {n=$2; sub(/:$/,"",n); print n; exit}')"
if [ -z "$IFACE" ]; then
  echo "could not find interface with MAC $MAC" >&2
  ip -o link show >&2
  exit 1
fi
echo "cabled interface: $IFACE"

# Drop any stale copy of this address, then add it.
ip addr del "$ADDR" dev "$IFACE" 2>/dev/null || true
ip addr add "$ADDR" dev "$IFACE"
ip link set "$IFACE" up
echo "assigned $ADDR to $IFACE"
ip -brief addr show "$IFACE"
