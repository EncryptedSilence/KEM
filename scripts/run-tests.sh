#!/usr/bin/env bash
# Run the unit-test suite via ctest for a given arch build. Exit nonzero on fail.
set -uo pipefail
ARCH="${1:-x86_64}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
CTEST="$REPO/.tools/cmake/bin/ctest"
[ -x "$CTEST" ] || CTEST="$(command -v ctest || true)"
cd "$REPO/build/$ARCH"
"$CTEST" --output-on-failure
rc=$?
echo "ctest rc=$rc"
exit $rc
