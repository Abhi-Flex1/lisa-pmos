#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

UI="${1:-phosh}"
OUTPUT_DIR="$REPO_ROOT/output"
IMAGES_DIR="$OUTPUT_DIR/images"
PACKAGES_DIR="$OUTPUT_DIR/packages"

echo "=================================================="
echo "  Building PostmarketOS for Xiaomi 11 Lite 5G NE  "
echo "  Target UI: $UI                                  "
echo "=================================================="

if ! command -v pmbootstrap &>/dev/null; then
    echo "Error: pmbootstrap is not installed in this environment."
    echo "If on macOS, run this inside the Docker/Colima container:"
    echo "  ./docker/run-pmbootstrap.sh /workspace/postmarket-lisa/scripts/build.sh $UI"
    exit 1
fi

# Apply patch to pmb to allow untrusted local signatures if needed
if [ -w /usr/lib/python3.*/site-packages/pmb/build/other.py ] 2>/dev/null; then
    sed -i "s/apk -q index/apk -q index --allow-untrusted/" /usr/lib/python3.*/site-packages/pmb/build/other.py 2>/dev/null || true
    sed -i "s/command_: list\[str\] = \[str(config.work \/ \"apk.static\"), \"--no-progress\"\]/command_: list[str] = [str(config.work \/ \"apk.static\"), \"--no-progress\", \"--allow-untrusted\"]/" /usr/lib/python3.*/site-packages/pmb/helpers/apk.py 2>/dev/null || true
elif sudo -n true 2>/dev/null; then
    sudo sed -i "s/apk -q index/apk -q index --allow-untrusted/" /usr/lib/python3.*/site-packages/pmb/build/other.py 2>/dev/null || true
    sudo sed -i "s/command_: list\[str\] = \[str(config.work \/ \"apk.static\"), \"--no-progress\"\]/command_: list[str] = [str(config.work \/ \"apk.static\"), \"--no-progress\", \"--allow-untrusted\"]/" /usr/lib/python3.*/site-packages/pmb/helpers/apk.py 2>/dev/null || true
fi

WORK_DIR="${PMB_WORK_DIR:-$HOME/.local/var/pmbootstrap}"
PMAPORTS_DIR="${PMAPORTS_DIR:-/home/builder/pmaports}"

mkdir -p "$WORK_DIR"
echo 8 > "$WORK_DIR/version"

if [ ! -d "$PMAPORTS_DIR" ]; then
    echo "==> Cloning pmaports shallowly..."
    for i in 1 2 3 4 5; do
        if [ ! -d "$PMAPORTS_DIR" ]; then
            git clone --depth 1 https://gitlab.postmarketos.org/postmarketOS/pmaports.git "$PMAPORTS_DIR" && break || { rm -rf "$PMAPORTS_DIR"; sleep 2; }
        fi
    done
fi

echo "==> Configuring pmbootstrap..."
python3 -c "
from pathlib import Path
from pmb.core import Config
from pmb.config import file
c = Config()
c.aports = [Path('$PMAPORTS_DIR')]
c.work = Path('$WORK_DIR')
c.device = 'xiaomi-lisa'
c.ui = '$UI'
c.user = 'user'
c.keymap = 'us'
c.timezone = 'UTC'
out = Path('$HOME/.config/pmbootstrap_v3.cfg')
out.parent.mkdir(parents=True, exist_ok=True)
file.save(out, c)
"

echo "==> Installing device and firmware packages to pmaports..."
mkdir -p "$PMAPORTS_DIR/device/testing"
rm -rf "$PMAPORTS_DIR/device/testing/device-xiaomi-lisa"
rm -rf "$PMAPORTS_DIR/device/testing/firmware-xiaomi-lisa"
cp -r "$REPO_ROOT/device/device-xiaomi-lisa" "$PMAPORTS_DIR/device/testing/"
cp -r "$REPO_ROOT/firmware/firmware-xiaomi-lisa" "$PMAPORTS_DIR/device/testing/"

echo "==> Integrating kernel assets for Xiaomi Lisa into linux-postmarketos-qcom-sc7280..."
KERNEL_PKG_DIR="$PMAPORTS_DIR/device/community/linux-postmarketos-qcom-sc7280"
cp "$REPO_ROOT/kernel/dts/sm7325-xiaomi-lisa.dts" "$KERNEL_PKG_DIR/"
cp "$REPO_ROOT/kernel/drivers/panel-csot-vtdr6130.c" "$KERNEL_PKG_DIR/"
cp "$REPO_ROOT/kernel/patches/0001-ASoC-qcom-sm8250-Add-Xiaomi-Lisa-soundcard-compatible.patch" "$KERNEL_PKG_DIR/"
python3 "$REPO_ROOT/scripts/patch-kernel-apkbuild.py" "$KERNEL_PKG_DIR/APKBUILD"

mkdir -p "$WORK_DIR/packages/edge/aarch64"
if sudo -n true 2>/dev/null; then
    sudo chown -R 12345:12345 "$WORK_DIR/packages" 2>/dev/null || true
fi

# Stage pre-built APKs if available to avoid recompiling kernel
if compgen -G "$PACKAGES_DIR/*.apk" > /dev/null; then
    echo "==> Staging existing pre-built packages from $PACKAGES_DIR..."
    cp "$PACKAGES_DIR"/*.apk "$WORK_DIR/packages/edge/aarch64/"
    pmbootstrap index
fi

echo "==> Generating checksums..."
pmbootstrap checksum firmware-xiaomi-lisa
pmbootstrap checksum device-xiaomi-lisa

if ! compgen -G "$WORK_DIR/packages/edge/aarch64/firmware-xiaomi-lisa*.apk" > /dev/null; then
    echo "==> Building firmware package..."
    pmbootstrap build firmware-xiaomi-lisa
fi

if ! compgen -G "$WORK_DIR/packages/edge/aarch64/linux-postmarketos-qcom-sc7280*.apk" > /dev/null; then
    echo "==> Building kernel package with Xiaomi Lisa additions..."
    pmbootstrap checksum linux-postmarketos-qcom-sc7280
    pmbootstrap build linux-postmarketos-qcom-sc7280
fi

if ! compgen -G "$WORK_DIR/packages/edge/aarch64/device-xiaomi-lisa*.apk" > /dev/null; then
    echo "==> Building device package..."
    pmbootstrap build device-xiaomi-lisa
fi

echo "==> Archiving all compiled APKs..."
mkdir -p "$PACKAGES_DIR"
cp "$WORK_DIR/packages/edge/aarch64"/*.apk "$PACKAGES_DIR/" 2>/dev/null || true

echo "==> Generating postmarketOS rootfs and boot images (UI: $UI)..."
pmbootstrap install --add android-tools --password 147147 --no-fde --ui "$UI"

TEMP_EXPORT="/tmp/pmos-export-$$"
mkdir -p "$TEMP_EXPORT"
echo "==> Exporting images..."
pmbootstrap export "$TEMP_EXPORT"

mkdir -p "$IMAGES_DIR"
rm -rf "$IMAGES_DIR"/*
cp -aL "$TEMP_EXPORT"/* "$IMAGES_DIR/" 2>/dev/null || true
rm -rf "$TEMP_EXPORT"

echo "=================================================="
echo "  Build successful! Artifacts exported to:        "
echo "    $IMAGES_DIR                                   "
echo "  To test boot on connected device:               "
echo "    ./scripts/test-boot.sh                        "
echo "=================================================="
