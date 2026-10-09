#!/usr/bin/env bash
# Fetch a portable CMake (no sudo) into .tools/cmake and print its bin path.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$REPO/.tools/cmake"

if [ -x "$DEST/bin/cmake" ]; then
  "$DEST/bin/cmake" --version | head -1
  echo "PATH:$DEST/bin"
  exit 0
fi

mkdir -p "$DEST"
curl -fsSL https://api.github.com/repos/Kitware/CMake/releases/latest -o /tmp/cm.json
LATEST="$(sed -nE 's/.*"tag_name": *"v?([0-9]+\.[0-9]+\.[0-9]+)".*/\1/p' /tmp/cm.json | head -1)"
[ -n "$LATEST" ] || { echo "could not determine latest cmake version" >&2; exit 1; }
echo ">> cmake $LATEST"
TB="cmake-${LATEST}-linux-x86_64.tar.gz"
URL="https://github.com/Kitware/CMake/releases/download/v${LATEST}/${TB}"
curl -fL --retry 3 -o "/tmp/$TB" "$URL"
tar -xzf "/tmp/$TB" -C "$DEST" --strip-components=1
"$DEST/bin/cmake" --version | head -1
echo "PATH:$DEST/bin"
