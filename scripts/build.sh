#!/usr/bin/env bash
# Build postmarketOS for the Xiaomi 11 Lite 5G NE (lisa).
# Default UI is KDE Plasma Mobile - pass another UI name as $1 to override.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

UI="${1:-${UI:-plasma-mobile}}"
OUTPUT_DIR="$REPO_ROOT/output"
IMAGES_DIR="$OUTPUT_DIR/images"
PACKAGES_DIR="$OUTPUT_DIR/packages"
PMAPORTS_DIR="${PMAPORTS_DIR:-$REPO_ROOT/pmaports}"
WORK_DIR="${PMB_WORK_DIR:-$REPO_ROOT/.pmos_cache}"
CONFIG_FILE="${PMB_CONFIG:-$HOME/.config/pmbootstrap_v3.cfg}"

echo "=================================================="
echo "  postmarketOS for Xiaomi 11 Lite 5G NE (lisa)"
echo "  UI: $UI"
echo "  pmaports: $PMAPORTS_DIR"
echo "  workdir:  $WORK_DIR"
echo "=================================================="

if ! command -v pmbootstrap &>/dev/null; then
    echo "Error: pmbootstrap not found. On macOS use:"
    echo "  ./docker/run-pmbootstrap.sh /workspace/postmarket-lisa/scripts/build.sh $UI"
    exit 1
fi

"$SCRIPT_DIR/validate.sh"

# ---------------------------------------------------------------------------
# Prepare pmaports
# ---------------------------------------------------------------------------
if [ ! -d "$PMAPORTS_DIR/.git" ]; then
    echo "==> Cloning pmaports into $PMAPORTS_DIR ..."
    git clone --depth 1 https://gitlab.postmarketos.org/postmarketOS/pmaports.git \
        "$PMAPORTS_DIR"
fi

mkdir -p "$PMAPORTS_DIR/device/testing" "$WORK_DIR"
# pmbootstrap 3.x workdir format marker (avoids a forced workdir migration)
echo 8 > "$WORK_DIR/version"

echo "==> Staging lisa packages into pmaports/device/testing ..."
for pkg in device-xiaomi-lisa firmware-xiaomi-lisa alsa-ucm-conf-lisa; do
    case "$pkg" in
        device-xiaomi-lisa)   src="$REPO_ROOT/device/device-xiaomi-lisa" ;;
        firmware-xiaomi-lisa) src="$REPO_ROOT/firmware/firmware-xiaomi-lisa" ;;
        alsa-ucm-conf-lisa)   src="$REPO_ROOT/audio/alsa-ucm-conf-lisa" ;;
    esac
    rm -rf "$PMAPORTS_DIR/device/testing/$pkg"
    cp -a "$src" "$PMAPORTS_DIR/device/testing/$pkg"
done

echo "==> Integrating kernel assets into linux-postmarketos-qcom-sc7280 ..."
# Start from the pristine upstream APKBUILD on every run so the patch is
# applied exactly once with the current assets/config flags.
git -C "$PMAPORTS_DIR" checkout -- \
    device/community/linux-postmarketos-qcom-sc7280/APKBUILD 2>/dev/null || true
KERNEL_PKG_DIR="$PMAPORTS_DIR/device/community/linux-postmarketos-qcom-sc7280"
cp "$REPO_ROOT/kernel/dts/sm7325-xiaomi-lisa.dts" "$KERNEL_PKG_DIR/"
cp "$REPO_ROOT/kernel/drivers/panel-csot-vtdr6130.c" "$KERNEL_PKG_DIR/"
cp "$REPO_ROOT/kernel/drivers/aw8624.c" "$KERNEL_PKG_DIR/"
cp "$REPO_ROOT/kernel/drivers/s5k3t2.c" "$KERNEL_PKG_DIR/"
cp "$REPO_ROOT/kernel/drivers/fpc1020.c" "$KERNEL_PKG_DIR/"
cp "$REPO_ROOT/kernel/patches/0001-ASoC-qcom-sm8250-Add-Xiaomi-Lisa-soundcard-compatible.patch" "$KERNEL_PKG_DIR/"
python3 "$SCRIPT_DIR/patch-kernel-apkbuild.py" "$KERNEL_PKG_DIR/APKBUILD" "$KERNEL_PKG_DIR"

# ---------------------------------------------------------------------------
# Write a deterministic pmbootstrap config (no interactive init needed)
# ---------------------------------------------------------------------------
echo "==> Writing $CONFIG_FILE ..."
python3 - "$CONFIG_FILE" "$PMAPORTS_DIR" "$WORK_DIR" "$UI" <<'PY'
import sys
from pathlib import Path
from pmb.core import Config
from pmb.config import file

out, aport, work, ui = sys.argv[1:5]
c = Config()
c.aports = [Path(aport)]
c.work = Path(work)
c.device = "xiaomi-lisa"
c.ui = ui
c.user = "user"
c.keymap = "us"
c.timezone = "UTC"
p = Path(out)
p.parent.mkdir(parents=True, exist_ok=True)
file.save(p, c)
PY

# The chroot builds as uid 12345 while this script copies as the host user;
# make the package dir writable by both so neither side gets "Permission denied".
mkdir -p "$WORK_DIR/packages/edge/aarch64"
sudo chmod -R a+rwX "$WORK_DIR/packages" 2>/dev/null || true

# Reuse previously built APKs when present (kernel rebuild is expensive).
if compgen -G "$PACKAGES_DIR/*.apk" > /dev/null; then
    echo "==> Staging cached APKs from $PACKAGES_DIR ..."
    cp "$PACKAGES_DIR"/*.apk "$WORK_DIR/packages/edge/aarch64/"
fi

# ---------------------------------------------------------------------------
# Build packages
# ---------------------------------------------------------------------------
# alsa-ucm-conf-qcom-sc7280 is a testing aport and must be built explicitly
# before our lisa UCM package (which depends on it).
for pkg in firmware-xiaomi-lisa alsa-ucm-conf-qcom-sc7280 alsa-ucm-conf-lisa device-xiaomi-lisa; do
    pmbootstrap checksum "$pkg"
    # --force avoids stale "up to date" decisions that skip a missing APK.
    pmbootstrap build "$pkg" --force
done

pmbootstrap checksum linux-postmarketos-qcom-sc7280
if ! compgen -G "$WORK_DIR/packages/edge/aarch64/linux-postmarketos-qcom-sc7280-"*.apk > /dev/null; then
    echo "==> Building kernel (this takes a while) ..."
    pmbootstrap build linux-postmarketos-qcom-sc7280
else
    echo "==> kernel already built, skipping."
fi

pmbootstrap index

# ---------------------------------------------------------------------------
# Rootfs + boot image
# ---------------------------------------------------------------------------
echo "==> Generating rootfs and boot.img with $UI ..."
"$SCRIPT_DIR/patch-apk-usrmerge.sh" "$WORK_DIR"
# apk occasionally reports a transient "failed to commit ...: No such file or
# directory" during usrmerge on a fresh chroot. The files are present; simply
# retrying the install against the partially populated rootfs completes it.
install_ok=0
for attempt in 1 2 3; do
    if pmbootstrap install --add android-tools,plasma-camera,libcamera \
        --password 147147 --no-fde; then
        install_ok=1
        break
    fi
    echo "==> pmbootstrap install attempt $attempt failed; retrying ($((3 - attempt)) left) ..."
done
if [ "$install_ok" -ne 1 ]; then
    echo "ERROR: pmbootstrap install failed after 3 attempts."
    exit 1
fi

TEMP_EXPORT="$(mktemp -d /tmp/pmos-export.XXXXXX)"
pmbootstrap export "$TEMP_EXPORT"
mkdir -p "$IMAGES_DIR"
rm -rf "$IMAGES_DIR"/*
cp -aL "$TEMP_EXPORT"/. "$IMAGES_DIR"/
if [ -f "$WORK_DIR/chroot_rootfs_xiaomi-lisa/boot/dtbo.img" ]; then
    cp -a "$WORK_DIR/chroot_rootfs_xiaomi-lisa/boot/dtbo.img" "$IMAGES_DIR/dtbo.img"
fi
rm -rf "$TEMP_EXPORT"

mkdir -p "$PACKAGES_DIR"
cp "$WORK_DIR/packages/edge/aarch64"/*.apk "$PACKAGES_DIR/" 2>/dev/null || true

echo "=================================================="
echo "  Build finished. Images: $IMAGES_DIR"
echo "  Test safely with: ./scripts/test-boot.sh"
echo "=================================================="
