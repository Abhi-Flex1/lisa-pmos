#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BOOT_IMG="${1:-$REPO_ROOT/output/images/boot.img}"
ROOTFS_IMG="${2:-$REPO_ROOT/output/images/xiaomi-lisa.img}"

if [ ! -f "$BOOT_IMG" ]; then
    BOOT_IMG="$(find "$REPO_ROOT/output" -name "boot.img" 2>/dev/null | head -n 1)"
fi
if [ ! -f "$ROOTFS_IMG" ]; then
    ROOTFS_IMG="$(find "$REPO_ROOT/output" -name "xiaomi-lisa.img" 2>/dev/null | head -n 1)"
fi

if [ -z "$BOOT_IMG" ] || [ ! -f "$BOOT_IMG" ]; then
    echo "Error: Missing boot.img. Usage: $0 [path/to/boot.img] [path/to/rootfs.img]"
    exit 1
fi

echo "=================================================="
echo "  WARNING: Flashing PostmarketOS to Xiaomi 11 Lite 5G NE  "
echo "  This will overwrite existing OS data!           "
echo "=================================================="
echo "Boot image:   $BOOT_IMG"
echo "Rootfs image: ${ROOTFS_IMG:-None provided}"
echo ""
read -p "Are you sure you want to continue? (yes/NO): " CONFIRM
if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

# Check device state
if ! fastboot devices | grep -q "[a-zA-Z0-9]"; then
    if adb devices | grep -v "List" | grep -q "device"; then
        echo "==> Rebooting to bootloader..."
        adb reboot bootloader
        fastboot wait-for-device
    else
        echo "Error: Device not detected in fastboot mode."
        exit 1
    fi
fi

echo "==> Flashing boot image to slot a and slot b..."
fastboot flash boot_a "$BOOT_IMG"
fastboot flash boot_b "$BOOT_IMG"

VENDOR_BOOT_IMG="$REPO_ROOT/output/images/vendor_boot.img"
if [ -f "$VENDOR_BOOT_IMG" ]; then
    echo "==> Flashing vendor_boot image to slot a and slot b..."
    fastboot flash vendor_boot_a "$VENDOR_BOOT_IMG"
    fastboot flash vendor_boot_b "$VENDOR_BOOT_IMG"
fi

if [ -n "$ROOTFS_IMG" ] && [ -f "$ROOTFS_IMG" ]; then
    echo "==> Flashing rootfs to userdata partition..."
    fastboot flash userdata "$ROOTFS_IMG"
fi

echo "==> Flashing complete! Rebooting device..."
fastboot reboot
