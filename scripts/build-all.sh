#!/usr/bin/env bash
# Configure + build the project for an architecture.
# Usage: build-all.sh <x86_64|armhf> [target]
set -euo pipefail

ARCH="${1:-x86_64}"
TARGET="${2:-all}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO"

# Portable cmake fetched by get-cmake.sh
CMAKE="$REPO/.tools/cmake/bin/cmake"
[ -x "$CMAKE" ] || CMAKE="$(command -v cmake || true)"
[ -n "$CMAKE" ] || { echo "cmake not found; run scripts/get-cmake.sh" >&2; exit 1; }

BUILD="build/$ARCH"
CFG_ARGS=(-S . -B "$BUILD" -DPQ_ARCH="$ARCH")
if [ "$ARCH" = "armhf" ]; then
  CFG_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$REPO/cmake/armhf-toolchain.cmake")
fi

"$CMAKE" "${CFG_ARGS[@]}"
if [ "$TARGET" = "all" ]; then
  "$CMAKE" --build "$BUILD" -j"$(nproc)"
else
  "$CMAKE" --build "$BUILD" --target "$TARGET" -j"$(nproc)"
fi
echo ">> build ($ARCH, $TARGET) complete -> $BUILD"
