#!/usr/bin/env bash
# Capture the environment of the node this runs on, into results/environment/.
# Usage: capture-env.sh <node-label> [arch]
set -uo pipefail
NODE="${1:-node}"
ARCH="${2:-x86_64}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT="$REPO/results/environment/${NODE}.txt"
mkdir -p "$(dirname "$OUT")"
PINNED="$REPO/install/$ARCH/openssl/bin/openssl"

{
  echo "# Environment: $NODE  ($(date -u +%Y-%m-%dT%H:%M:%SZ))"
  echo "## uname"; uname -a; echo "arch=$(uname -m) LONG_BIT=$(getconf LONG_BIT)"
  echo "## os-release"; grep -E "PRETTY_NAME|VERSION" /etc/os-release 2>/dev/null
  echo "## cpu"; grep -E "model name|Hardware|Features|CPU part|processor" /proc/cpuinfo 2>/dev/null | sort -u
  echo "ncpu=$(nproc)"
  echo "## memory"; free -h 2>/dev/null; echo "--- swaps ---"; cat /proc/swaps 2>/dev/null
  echo "## disk"; df -h / 2>/dev/null
  echo "## compiler"; (gcc --version 2>/dev/null || cc --version 2>/dev/null) | head -1
  echo "arm-linux-gnueabihf-gcc: $(arm-linux-gnueabihf-gcc --version 2>/dev/null | head -1 || echo n/a)"
  echo "## openssl (system)"; openssl version -a 2>/dev/null | head -2
  echo "## openssl (pinned, if present)"
  if [ -x "$PINNED" ]; then "$PINNED" version; else echo "pinned not present on this node"; fi
  echo "## network"; ip -brief addr 2>/dev/null || ifconfig -a 2>/dev/null | head -20
} > "$OUT" 2>&1
echo "wrote $OUT"
