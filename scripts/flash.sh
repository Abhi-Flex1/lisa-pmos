#!/usr/bin/env bash
# Flash the built postmarketOS images to a Xiaomi 11 Lite 5G NE (lisa).
# This device uses GKI boot images (boot + vendor_boot) and requires the
# vbmeta verification flags to be cleared before a custom boot image loads.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BOOT_IMG="${1:-$REPO_ROOT/output/images/boot.img}"
ROOTFS_IMG="${2:-$REPO_ROOT/output/images/xiaomi-lisa.img}"
VENDOR_BOOT_IMG="${3:-$REPO_ROOT/output/images/vendor_boot.img}"

[ -f "$BOOT_IMG" ] || { echo "Missing boot.img: $BOOT_IMG"; exit 1; }
[ -f "$ROOTFS_IMG" ] || { echo "Missing rootfs image: $ROOTFS_IMG"; exit 1; }
[ -f "$VENDOR_BOOT_IMG" ] || { echo "Missing vendor_boot.img: $VENDOR_BOOT_IMG"; exit 1; }

echo "=================================================="
echo "  WARNING: this overwrites boot_a/b, vbmeta_a/b and userdata"
echo "  Boot:       $BOOT_IMG"
echo "  VendorBoot: $VENDOR_BOOT_IMG"
echo "  Rootfs:     $ROOTFS_IMG"
echo "=================================================="
read -r -p "Type 'yes' to continue: " CONFIRM
[ "$CONFIRM" = "yes" ] || { echo "Aborted."; exit 0; }

# Make sure the device is in fastboot
if ! timeout 10 fastboot devices 2>/dev/null | grep -q .; then
    if adb devices 2>/dev/null | grep -v List | grep -q device; then
        adb reboot bootloader
        for i in $(seq 1 30); do
            timeout 2 fastboot devices 2>/dev/null | grep -q . && break
            sleep 1
        done
    fi
fi
timeout 10 fastboot devices | grep -q . || { echo "No fastboot device"; exit 1; }

# vbmeta with AVB verification disabled (postmarketOS' own format: flags=2)
VBMETA="$(mktemp /tmp/pmos-vbmeta.XXXXXX.img)"
avbtool make_vbmeta_image --flags 2 --padding_size 4096 --output "$VBMETA"

echo "==> Flashing boot images (both slots) ..."
for p in boot_a boot_b; do fastboot flash "$p" "$BOOT_IMG"; done
echo "==> Flashing vendor_boot images (both slots) ..."
for p in vendor_boot_a vendor_boot_b; do fastboot flash "$p" "$VENDOR_BOOT_IMG"; done
echo "==> Disabling AVB verification ..."
for p in vbmeta_a vbmeta_b; do fastboot flash "$p" "$VBMETA"; done
rm -f "$VBMETA"

echo "==> Flashing rootfs to userdata ..."
fastboot flash userdata "$ROOTFS_IMG"

fastboot --set-active=a
echo "==> Rebooting ..."
fastboot reboot
