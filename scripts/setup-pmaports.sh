#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PMBOOTSTRAP_DIR="${1:-$HOME/.local/var/pmbootstrap/cache_git/pmaports}"

if [ ! -d "$PMBOOTSTRAP_DIR" ]; then
    echo "Warning: pmaports directory '$PMBOOTSTRAP_DIR' not found."
    echo "Run 'pmbootstrap init' first, or pass the pmaports directory as an argument:"
    echo "  $0 /path/to/pmaports"
    exit 1
fi

echo "==> Linking device-xiaomi-lisa and firmware-xiaomi-lisa into pmaports..."
mkdir -p "$PMBOOTSTRAP_DIR/device/testing"

# Remove existing symlinks/directories if present
rm -rf "$PMBOOTSTRAP_DIR/device/testing/device-xiaomi-lisa"
rm -rf "$PMBOOTSTRAP_DIR/device/testing/firmware-xiaomi-lisa"

cp -r "$REPO_ROOT/device/device-xiaomi-lisa" "$PMBOOTSTRAP_DIR/device/testing/"
cp -r "$REPO_ROOT/firmware/firmware-xiaomi-lisa" "$PMBOOTSTRAP_DIR/device/testing/"

echo "==> Done! Packages are now available in pmaports testing."
