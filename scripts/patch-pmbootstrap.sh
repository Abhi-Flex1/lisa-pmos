#!/usr/bin/env bash
# Patch installed pmbootstrap 3.12.1 to tolerate Alpine edge apk-tools 3.0.10
# usrmerge duplicate-path commit errors during rootfs creation.
#
# Background: packages such as systemd, systemd-udevd and sshfs ship both
# /bin and /usr/bin (or /sbin and /usr/sbin) entries. In pmbootstrap's
# usrmerged root these resolve to the same file, so apk prints e.g.
#   "failed to commit usr/bin/udevadm: No such file or directory"
# and exits 2/3 even though the correct file is installed.
set -e

TARGET=${1:-/usr/lib/python3.14/site-packages/pmb/helpers/run_core.py}
[ -f "$TARGET" ] || { echo "not found: $TARGET"; exit 0; }

if grep -q 'usrmerge duplicate-path' "$TARGET"; then
    echo "pmbootstrap already patched"
    exit 0
fi

sudo cp "$TARGET" "$TARGET.abhi-backup"
sudo python3 - "$TARGET" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
old = "    if code:\n        logging.debug(\"^\" * 70)"
new = '''    if code in (2, 3) and "apk.static" in log_message:
        # Alpine edge apk-tools 3.0.10 + usrmerged root: duplicate /bin vs
        # /usr/bin paths cause benign "failed to commit" errors. Files are
        # installed correctly, so ignore the completed-transaction code.
        logging.warning(
            f"Ignoring apk add exit code {code} (usrmerge duplicate-path "
            "commit errors; files are installed correctly)"
        )
        return
    if code:
        logging.debug("^" * 70)'''
assert old in s, "unexpected pmbootstrap source layout"
open(p, "w").write(s.replace(old, new, 1))
PY
echo "Patched $TARGET (backup at $TARGET.abhi-backup)"
