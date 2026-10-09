#!/usr/bin/env bash
# Sourced helper: provides pi_ssh / pi_rsync_to for reaching the Orange Pi.
# Copies the SSH key into the WSL-native ~/.ssh (drvfs perms are too open for
# ssh to accept the key directly from /mnt/c).
#   PI_HOST  (default 10.42.0.2 — the static point-to-point IP)
#   PI_USER  (default root)
#   PI_SRC_KEY (default /mnt/c/Users/Leo/.ssh/pqc_orangepi)
PI_HOST="${PI_HOST:-10.42.0.2}"
PI_USER="${PI_USER:-root}"
PI_SRC_KEY="${PI_SRC_KEY:-/mnt/c/Users/Leo/.ssh/pqc_orangepi}"
WSL_KEY="$HOME/.ssh/pqc_orangepi"

mkdir -p "$HOME/.ssh"; chmod 700 "$HOME/.ssh"
if [ ! -f "$WSL_KEY" ] && [ -f "$PI_SRC_KEY" ]; then cp "$PI_SRC_KEY" "$WSL_KEY"; fi
chmod 600 "$WSL_KEY" 2>/dev/null || true

PI_SSH_OPTS="-i $WSL_KEY -o IdentitiesOnly=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 -o BatchMode=yes"

pi_ssh()      { ssh $PI_SSH_OPTS "$PI_USER@$PI_HOST" "$@"; }
pi_rsync_to() { rsync -az -e "ssh $PI_SSH_OPTS" "$@" "$PI_USER@$PI_HOST:"; }
