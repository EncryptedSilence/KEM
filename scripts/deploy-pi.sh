#!/usr/bin/env bash
# Copy the armhf binaries + certs (incl. the Pi's own private key, over SSH)
# to the Orange Pi at ~/pqc/. Run from WSL after cross-building.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/pi-ssh.sh"

BIN="$REPO/build/armhf"
C="$REPO/certs"
[ -x "$BIN/pq_node" ] || { echo "armhf pq_node missing; run build-all.sh armhf" >&2; exit 1; }

echo ">> preparing ~/pqc on the Pi"
pi_ssh 'mkdir -p ~/pqc/certs/private ~/pqc/results/raw ~/pqc/results/environment'

echo ">> copying binaries + certs"
pi_rsync_to "$BIN/pq_node" "$BIN/kem_bench"
pi_ssh 'mv ~/pq_node ~/pqc/ && mv ~/kem_bench ~/pqc/ && chmod +x ~/pqc/pq_node ~/pqc/kem_bench'
rsync -az -e "ssh $PI_SSH_OPTS" "$C/ca.crt" "$C/opi-node.crt" "$PI_USER@$PI_HOST:pqc/certs/"
rsync -az -e "ssh $PI_SSH_OPTS" "$C/private/opi-node.key" "$PI_USER@$PI_HOST:pqc/certs/private/"
# also send the scripts the Pi runs
rsync -az -e "ssh $PI_SSH_OPTS" "$SCRIPT_DIR/capture-env.sh" "$PI_USER@$PI_HOST:pqc/"
pi_ssh 'ls -l ~/pqc ~/pqc/certs ~/pqc/certs/private; ~/pqc/pq_node --help 2>&1 | head -1 || true; ~/pqc/kem_bench --help 2>&1 | head -1 || true'
echo ">> deploy complete"
