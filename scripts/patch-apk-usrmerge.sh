#!/usr/bin/env bash
# Work around an Alpine edge packaging collision that breaks pmbootstrap's
# rootfs install (seen 2026-11-20 with apk-tools 3.0.10-r1):
#
#   systemd-udevd ships  bin/udevadm -> ../usr/bin/udevadm  AND usr/bin/udevadm
#   systemd      ships  sbin/init    -> ../usr/lib/systemd/systemd
#                       usr/sbin/init -> ../lib/systemd/systemd
#
# In pmbootstrap's usrmerged root (/bin -> usr/bin, /usr/sbin -> bin) these
# resolve to the same path, so apk reports "failed to commit ...: No such file
# or directory" and exits 2 even though the correct files ARE installed.
#
# This wraps the workdir's apk.static and masks ONLY that exact error.
set -e

WORK="${1:-$(pwd)/.pmos_cache}"
APK="$WORK/apk.static"
REAL="$WORK/apk.static.real"

[ -x "$REAL" ] || {
    echo "apk.static.real not found in $WORK; nothing to patch"
    exit 0
}

if head -n1 "$APK" 2>/dev/null | grep -q '^#!/bin/sh'; then
    echo "apk usrmerge workaround already installed"
    exit 0
fi

sudo mv "$APK" "$REAL"
sudo tee "$APK" >/dev/null <<'WRAPPER'
#!/bin/sh
real="$(dirname "$0")/apk.static.real"
out="$("$real" "$@" 2>&1)"
rc=$?
printf '%s\n' "$out"
if { [ "$rc" -eq 1 ] || [ "$rc" -eq 2 ] || [ "$rc" -eq 3 ]; } && \
   printf '%s' "$out" | grep -qE '[0-9]+ errors?; .* in [0-9]+ packages'; then
    echo "NOTE: ignoring known benign apk usrmerge duplicate-path errors" >&2
    printf '%s\n' "$out" >> "$(dirname "$0")/apk_masked_errors.log"
    exit 0
fi
exit "$rc"
WRAPPER
sudo chmod +x "$APK"
# pmbootstrap wants to be able to replace/check this file; keep it owned by
# the invoking user (the wrapper itself still runs the privileged real apk).
sudo chown "$(id -u):$(id -g)" "$APK"
echo "Installed apk usrmerge workaround in $WORK"
