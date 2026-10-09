#!/usr/bin/env bash
# Fetch a self-contained armv7 (armhf) cross-toolchain from Bootlin — no sudo.
# Prints: export lines for PQ_CROSS_PREFIX and PATH. `eval "$(get-cross.sh)"`.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$REPO/.tools/cross-armhf"
TB="armv7-eabihf--glibc--stable-2026.08-1.tar.xz"
URL="https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs/$TB"

find_gcc() { find "$DEST" -maxdepth 3 \( -type f -o -type l \) -name 'arm-*-linux-gnueabihf-gcc' 2>/dev/null | head -1; }

if [ -z "$(find_gcc)" ]; then
  mkdir -p "$DEST"
  echo ">> downloading Bootlin toolchain" >&2
  curl -fL --retry 3 -o "/tmp/$TB" "$URL" >&2
  tar -xf "/tmp/$TB" -C "$DEST"
  SDK="$(dirname "$(dirname "$(find_gcc)")")"
  if [ -x "$SDK/relocate-sdk.sh" ]; then ( cd "$SDK" && ./relocate-sdk.sh ) >&2; fi
fi

GCC="$(find_gcc)"
[ -n "$GCC" ] || { echo "cross gcc not found after extract" >&2; exit 1; }
# Print the ABSOLUTE compiler prefix (…/bin/arm-buildroot-linux-gnueabihf-).
# Using an absolute prefix avoids any PATH manipulation (the PATH here contains
# "(x86)" which breaks naive eval). Callers: PQ_CROSS_PREFIX=$(get-cross.sh)
echo "${GCC%gcc}"
