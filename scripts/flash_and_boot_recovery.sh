#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

echo "Waiting for fastboot device..."
while true; do
    if fastboot devices 2>&1 | grep -E '[a-zA-Z0-9]+[[:space:]]+fastboot'; then
        echo "Device detected in fastboot!"
        break
    fi
    sleep 0.5
done

echo "Flashing verified working stack..."
fastboot flash vbmeta_a output/images/lineage_vbmeta.img
fastboot flash vbmeta_b output/images/lineage_vbmeta.img
fastboot flash dtbo_a output/images/lineage_dtbo.img
fastboot flash dtbo_b output/images/lineage_dtbo.img
fastboot flash vendor_boot_a output/images/current_vendor_boot.img
fastboot flash vendor_boot_b output/images/current_vendor_boot.img
fastboot flash boot_a output/images/current_boot.img
fastboot flash boot_b output/images/current_boot.img
fastboot --set-active=a

echo "Rebooting to Recovery (this will turn ON screen, AMOLED, touch & ADB)..."
fastboot reboot recovery
echo "Recovery boot command sent!"
