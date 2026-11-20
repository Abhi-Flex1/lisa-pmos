#!/usr/bin/env bash
# Link all lisa aport directories into an existing pmaports checkout.
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

echo "==> Linking lisa packages into $PMBOOTSTRAP_DIR/device/testing ..."
mkdir -p "$PMBOOTSTRAP_DIR/device/testing"

link_pkg() {
    src="$1"; name="$2"
    rm -rf "$PMBOOTSTRAP_DIR/device/testing/$name"
    cp -a "$src" "$PMBOOTSTRAP_DIR/device/testing/$name"
    echo "    + $name"
}

link_pkg "$REPO_ROOT/device/device-xiaomi-lisa"     device-xiaomi-lisa
link_pkg "$REPO_ROOT/firmware/firmware-xiaomi-lisa" firmware-xiaomi-lisa
link_pkg "$REPO_ROOT/audio/alsa-ucm-conf-lisa"      alsa-ucm-conf-lisa

echo "==> Integrating kernel assets ..."
KDIR="$PMBOOTSTRAP_DIR/device/community/linux-postmarketos-qcom-sc7280"
cp "$REPO_ROOT/kernel/dts/sm7325-xiaomi-lisa.dts" "$KDIR/"
cp "$REPO_ROOT/kernel/drivers/panel-csot-vtdr6130.c" "$KDIR/"
cp "$REPO_ROOT/kernel/patches/0001-ASoC-qcom-sm8250-Add-Xiaomi-Lisa-soundcard-compatible.patch" "$KDIR/"
python3 "$SCRIPT_DIR/patch-kernel-apkbuild.py" "$KDIR/APKBUILD" "$KDIR"

echo "==> Done."
