#!/usr/bin/env bash
# Build OpenSSL 3.5.9 (pinned, sha256-verified) into a project-local prefix.
# Usage: build-openssl.sh <x86_64|armhf>
# Runs inside WSL (x86_64 native) or cross-compiles for armhf with
# arm-linux-gnueabihf-. Static libs only (no-shared no-dso).
set -euo pipefail

ARCH="${1:-}"
case "$ARCH" in
  x86_64|armhf) ;;
  *) echo "usage: $0 <x86_64|armhf>" >&2; exit 2 ;;
esac

VER="3.5.9"
TARBALL="openssl-${VER}.tar.gz"
URL="https://github.com/openssl/openssl/releases/download/openssl-${VER}/${TARBALL}"
SHA256="603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
CACHE="$REPO/3rdparty/openssl"
PREFIX="$REPO/install/$ARCH/openssl"
WORK="${PQ_WORK:-/tmp/pqc-openssl-$ARCH}"   # build on native FS for speed

mkdir -p "$CACHE" "$PREFIX" "$WORK"

# 1. Download (cached) -------------------------------------------------------
if [ ! -f "$CACHE/$TARBALL" ]; then
  echo ">> downloading $TARBALL"
  if command -v curl >/dev/null; then curl -fL --retry 3 -o "$CACHE/$TARBALL" "$URL";
  else wget -O "$CACHE/$TARBALL" "$URL"; fi
fi

# 2. Verify sha256 (abort on mismatch) --------------------------------------
echo ">> verifying sha256"
echo "${SHA256}  ${CACHE}/${TARBALL}" | sha256sum -c -

# 3. Extract ----------------------------------------------------------------
rm -rf "$WORK/src"
mkdir -p "$WORK/src"
tar -xzf "$CACHE/$TARBALL" -C "$WORK/src" --strip-components=1

# 4. Configure --------------------------------------------------------------
cd "$WORK/src"
COMMON=(no-shared no-dso no-tests no-docs no-apps threads
        --prefix="$PREFIX" --openssldir="$PREFIX/ssl" --libdir=lib)
# keep apps? we need bin/openssl for cert generation on x86_64 only.
if [ "$ARCH" = "x86_64" ]; then
  # drop no-apps so we get bin/openssl for make-certs.sh
  CFG=(no-shared no-dso no-tests no-docs threads
       --prefix="$PREFIX" --openssldir="$PREFIX/ssl" --libdir=lib)
  ./Configure linux-x86_64 "${CFG[@]}"
else
  export CROSS_COMPILE=arm-linux-gnueabihf-
  ./Configure linux-armv4 no-shared no-dso no-tests no-docs threads \
      --prefix="$PREFIX" --openssldir="$PREFIX/ssl" --libdir=lib
fi

# 5. Build + install (libs + headers) ---------------------------------------
NPROC="$(nproc)"
echo ">> building with -j$NPROC"
make -j"$NPROC"
make install_sw

# 6. Report -----------------------------------------------------------------
echo ">> installed to $PREFIX"
ls -l "$PREFIX/lib/libssl.a" "$PREFIX/lib/libcrypto.a"
if [ "$ARCH" = "x86_64" ]; then
  "$PREFIX/bin/openssl" version
fi
echo ">> OpenSSL $VER ($ARCH) build complete"
