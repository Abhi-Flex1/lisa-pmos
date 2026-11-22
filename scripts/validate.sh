#!/usr/bin/env bash
# Static validation of the lisa port. Safe to run on the host, no root needed.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PMAPORTS="${PMAPORTS_DIR:-$REPO_ROOT/pmaports}"
fails=0

pass() { printf '  [ OK ] %s\n' "$*"; }
fail() { printf '  [FAIL] %s\n' "$*"; fails=$((fails + 1)); }
check() { if "$@" >/dev/null 2>&1; then pass "$*"; else fail "$*"; fi; }

echo "==> Shell/Python syntax"
while IFS= read -r f; do
    check bash -n "$f"
done < <(find "$REPO_ROOT/scripts" "$REPO_ROOT/docker" -type f -name '*.sh' -print)
check python3 -m py_compile "$SCRIPT_DIR/patch-kernel-apkbuild.py"
check python3 -m py_compile "$SCRIPT_DIR/patch-boot-dtb-rotation.py"
if [ -f "$REPO_ROOT/fingerprint/qseecom_probe.py" ]; then
    check python3 -m py_compile "$REPO_ROOT/fingerprint/qseecom_probe.py"
fi

echo "==> APKBUILD checksums"
check_aport() {
    local dir="$1" pkg="$2"
    [ -f "$dir/APKBUILD" ] || { fail "$pkg: APKBUILD missing"; return; }
    local sums line file expected actual
    sums="$(sed -n '/^sha512sums="/,/^"/p' "$dir/APKBUILD")"
    while read -r expected file; do
        [ -n "${expected:-}" ] || continue
        case "$file" in
            http://*|https://*|*.tar.gz|*.tar.bz2|*.tar.xz) continue ;;
        esac
        [ -f "$dir/$file" ] || { fail "$pkg: missing source $file"; continue; }
        actual="$(sha512sum "$dir/$file" | cut -d' ' -f1)"
        if [ "$actual" = "$expected" ]; then
            pass "$pkg: $file sha512"
        else
            fail "$pkg: $file sha512 mismatch"
        fi
    done <<< "$(printf '%s\n' "$sums" | grep -E '^[[:xdigit:]]{128}  ' || true)"
}
check_aport "$REPO_ROOT/device/device-xiaomi-lisa" device-xiaomi-lisa
check_aport "$REPO_ROOT/firmware/firmware-xiaomi-lisa" firmware-xiaomi-lisa
check_aport "$REPO_ROOT/audio/alsa-ucm-conf-lisa" alsa-ucm-conf-lisa
check_aport "$REPO_ROOT/camera/libcamera-lisa" libcamera-lisa

echo "==> deviceinfo"
DI="$REPO_ROOT/device/device-xiaomi-lisa/deviceinfo"
for kv in \
    'deviceinfo_codename="xiaomi-lisa"' \
    'deviceinfo_arch="aarch64"' \
    'deviceinfo_dtb="qcom/sm7325-xiaomi-lisa"' \
    'deviceinfo_append_dtb="true"' \
    'deviceinfo_header_version="0"' \
    'deviceinfo_flash_offset_base="0x00000000"' \
    'deviceinfo_kernel_cmdline="earlycon console=tty0"' \
    'deviceinfo_flash_fastboot_partition_dtbo="dtbo"' \
    'deviceinfo_drm="true"' \
    'deviceinfo_flash_method="fastboot"' \
    'deviceinfo_super_partitions="/dev/sda20 /dev/sda20"'; do
    if grep -Fxq "$kv" "$DI"; then pass "deviceinfo: $kv"; else fail "deviceinfo: $kv"; fi
done
for dep in 'linux-firmware-ath11k' 'hexagonrpcd>=0.3.2-r3' 'mkbootimg'; do
    if grep -Fq "$dep" "$REPO_ROOT/device/device-xiaomi-lisa/APKBUILD"; then
        pass "device APKBUILD depends on $dep"
    else
        fail "device APKBUILD missing $dep"
    fi
done

echo "==> Firmware manifest coverage"
FW_REPO="${FW_REPO:-$REPO_ROOT/work/firmware-xiaomi-lisa}"
if [ -d "$FW_REPO/.git" ]; then
    python3 - "$FW_REPO" "$REPO_ROOT/firmware/firmware-xiaomi-lisa" <<'PY'
import sys
from pathlib import Path
repo, aport = Path(sys.argv[1]), Path(sys.argv[2])
files = {str(p.relative_to(repo)) for p in repo.rglob('*')
         if p.is_file() and '.git' not in p.parts}
manifest = {l.strip().removeprefix('./')
            for f in aport.glob('*.files')
            for l in f.read_text().splitlines() if l.strip()}
if files == manifest:
    print(f'  [ OK ] all {len(files)} firmware files are packaged')
else:
    print(f'  [FAIL] firmware manifest mismatch: missing={len(files-manifest)} '
          f'extra={len(manifest-files)}')
    raise SystemExit(1)
PY
else
    echo "  [ SKIP ] firmware reference clone not present ($FW_REPO)"
fi

echo "==> Kernel assets"
DTS="$REPO_ROOT/kernel/dts/sm7325-xiaomi-lisa.dts"
for needle in 'compatible = "xiaomi,lisa"' 'compatible = "csot,vtdr6130"' \
              'compatible = "goodix,gt9897"' 'xiaomi,lisa-sndcard' \
              'qcom,wcn6750-bt'; do
    if grep -Fq "$needle" "$DTS"; then pass "DTS contains $needle"; else fail "DTS missing $needle"; fi
done
PATCH="$REPO_ROOT/kernel/patches/0001-ASoC-qcom-sm8250-Add-Xiaomi-Lisa-soundcard-compatible.patch"
if grep -Fq 'xiaomi,lisa-sndcard' "$PATCH"; then pass "soundcard patch present"; else fail "soundcard patch broken"; fi
if grep -q 'sm7325-xiaomi-lisa.dts' "$SCRIPT_DIR/patch-kernel-apkbuild.py" && \
   grep -q 'patch -p1 -i' "$SCRIPT_DIR/patch-kernel-apkbuild.py"; then
    pass "kernel patch script applies the soundcard patch"
else
    fail "kernel patch script does not apply the soundcard patch"
fi

echo "==> UI / build wiring"
if [ -d "$PMAPORTS/main/postmarketos-ui-plasma-mobile" ]; then
    pass "pmaports has postmarketos-ui-plasma-mobile"
else
    fail "pmaports UI package missing"
fi
if grep -q 'UI="${1:-${UI:-plasma-mobile}}"' "$SCRIPT_DIR/build.sh"; then
    pass "default UI is plasma-mobile"
else
    fail "default UI is not plasma-mobile"
fi
if grep -q 'camera/libcamera-lisa' "$SCRIPT_DIR/build.sh"; then
    pass "build.sh stages the calibrated libcamera fork"
else
    fail "build.sh does not stage the calibrated libcamera fork"
fi
if grep -q 'pmbootstrap install .*--ui' "$SCRIPT_DIR/build.sh"; then
    fail "build.sh passes unsupported --ui to pmbootstrap install"
else
    pass "build.sh does not pass unsupported --ui"
fi

echo
if [ "$fails" -eq 0 ]; then
    echo "VALIDATION: all checks passed"
    exit 0
fi
echo "VALIDATION: $fails check(s) failed"
exit 1
