#!/usr/bin/env bash
# Capture read-only diagnostics from a booted lisa device (ADB or SSH).
# Usage: ./scripts/device-report.sh [adb|ssh|ssh-user@host]
set -uo pipefail
MODE="${1:-adb}"
OUT="${2:-device-report.txt}"

run() {
    echo
    echo "===== $* ====="
    case "$MODE" in
        adb) adb shell "$*" 2>&1 ;;
        ssh) ssh user@172.16.42.1 "$*" 2>&1 ;;
        ssh-*) ssh "${MODE#ssh-}" "$*" 2>&1 ;;
        *) echo "unknown mode: $MODE" >&2; return 1 ;;
    esac
}

{
    echo "postmarketOS lisa device report - $(date -u +%FT%TZ)"
    echo "mode: $MODE"
    run 'uname -a'
    run 'cat /etc/os-release | head -5'
    run 'ip link show'
    run 'ip addr show | grep -E "inet |state "'
    run 'ls -l /dev/dri/ 2>/dev/null || echo NO_DRI'
    run 'cat /sys/class/drm/card0-DSI-1/status 2>/dev/null || echo NO_DSI_STATUS'
    run 'cat /proc/asound/cards 2>/dev/null || echo NO_SOUND_CARD'
    run 'alsa-info --no-upload 2>/dev/null | head -20 || true'
    run 'dmesg | grep -iE "goodix|panel|mdss|drm|ath11k|wcn|q6|modem|rmtfs|fatal|error" | tail -80'
    run 'mmcli -L 2>/dev/null || echo NO_MODEMMANAGER'
    run 'ls /usr/share/alsa/ucm2/conf.d/sm7325/ 2>/dev/null || echo NO_UCM'
    run 'cat /proc/uptime'
} | tee "$OUT"

echo "Report saved to $OUT"
