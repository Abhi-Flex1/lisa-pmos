#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BOOT_IMG="${1:-$REPO_ROOT/output/images/boot.img}"
if [ ! -f "$BOOT_IMG" ]; then
    BOOT_IMG="$(find "$REPO_ROOT/output" -name "boot.img" 2>/dev/null | head -n 1)"
fi

if [ -z "$BOOT_IMG" ] || [ ! -f "$BOOT_IMG" ]; then
    echo "Usage: $0 [/path/to/boot.img]"
    echo "No boot.img found in $REPO_ROOT/output."
    exit 1
fi

echo "==> Using boot image: $BOOT_IMG"

# Check device state
if fastboot devices | grep -q "[a-zA-Z0-9]"; then
    echo "==> Fastboot device detected."
elif adb devices | grep -v "List" | grep -q "device"; then
    echo "==> Device detected in ADB mode. Rebooting to bootloader..."
    adb reboot bootloader
    echo "==> Waiting for device in fastboot mode..."
    for i in $(seq 1 30); do
        fastboot devices 2>/dev/null | grep -q . && break
        sleep 1
    done
else
    echo "Error: No device detected in ADB or Fastboot mode."
    echo "Please connect your Xiaomi 11 Lite 5G NE and enable USB debugging or hold Power + Vol Down."
    exit 1
fi

echo "==> Booting test image non-destructively (will NOT overwrite internal storage)..."
fastboot boot "$BOOT_IMG"

echo "==> Boot command sent! Monitor device screen and USB networking."
echo "If USB networking is up, you can connect via:"
echo "  ssh user@172.16.42.1"
