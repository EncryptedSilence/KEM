#!/usr/bin/env bash
# Generate a local test CA (ECDSA P-256) and node certs for mutual TLS auth.
# Public certs -> certs/ ; private keys -> certs/private/ (gitignored).
# A 'rogue' self-signed cert (NOT signed by the CA) is produced for the
# negative tests. Uses the pinned OpenSSL 3.5.9.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
OSSL="$REPO/install/x86_64/openssl/bin/openssl"
[ -x "$OSSL" ] || { echo "pinned openssl not found: $OSSL (run build-openssl.sh x86_64)" >&2; exit 1; }

CERTS="$REPO/certs"
PRIV="$CERTS/private"
mkdir -p "$CERTS" "$PRIV"
chmod 700 "$PRIV"

# Minimal config so `req` never prompts or hunts for a system openssl.cnf.
CNF="$CERTS/openssl.cnf"
cat > "$CNF" <<'EOF'
[req]
distinguished_name = dn
prompt = no
[dn]
CN = placeholder
EOF
export OPENSSL_CONF="$CNF"

p256() { "$OSSL" genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out "$1"; }

# --- CA --------------------------------------------------------------------
p256 "$PRIV/ca.key"
"$OSSL" req -x509 -new -key "$PRIV/ca.key" -subj "/CN=pqc-test-ca" \
    -days 3650 -sha256 \
    -addext "basicConstraints=critical,CA:TRUE,pathlen:0" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" \
    -out "$CERTS/ca.crt"

# --- leaf helper -----------------------------------------------------------
gen_leaf() {
  local name="$1" san="$2"
  p256 "$PRIV/$name.key"
  "$OSSL" req -new -key "$PRIV/$name.key" -subj "/CN=$name" -out "/tmp/$name.csr"
  cat > "/tmp/$name.ext" <<EOF
subjectAltName=$san
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature
extendedKeyUsage=serverAuth,clientAuth
EOF
  "$OSSL" x509 -req -in "/tmp/$name.csr" -CA "$CERTS/ca.crt" -CAkey "$PRIV/ca.key" \
      -CAcreateserial -days 3650 -sha256 -extfile "/tmp/$name.ext" \
      -out "$CERTS/$name.crt"
  rm -f "/tmp/$name.csr" "/tmp/$name.ext"
}

gen_leaf pc-node  "DNS:pc-node,IP:10.42.0.1"
gen_leaf opi-node "DNS:opi-node,IP:10.42.0.2"

# --- rogue (self-signed, impersonates pc-node, NOT from our CA) ------------
p256 "$PRIV/rogue.key"
"$OSSL" req -x509 -new -key "$PRIV/rogue.key" -subj "/CN=pc-node" -days 3650 -sha256 \
    -addext "subjectAltName=DNS:pc-node" -out "$CERTS/rogue.crt"

chmod 600 "$PRIV"/*.key

# --- verify ----------------------------------------------------------------
echo ">> chain verification (expect OK):"
"$OSSL" verify -CAfile "$CERTS/ca.crt" "$CERTS/pc-node.crt" "$CERTS/opi-node.crt"
echo ">> rogue verification (expect failure):"
if "$OSSL" verify -CAfile "$CERTS/ca.crt" "$CERTS/rogue.crt" 2>/dev/null; then
  echo "ERROR: rogue cert unexpectedly verified" >&2; exit 1
else
  echo "rogue correctly rejected"
fi
echo ">> certs written to $CERTS (keys in $PRIV, gitignored)"
