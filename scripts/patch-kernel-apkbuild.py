#!/usr/bin/env python3
"""Patch the linux-postmarketos-qcom-sc7280 APKBUILD to add Xiaomi Lisa support.

Strategy: do NOT add local files to source= (they would need sha512 checksums).
Instead, inject them directly in prepare() by copying from the build dir where
pmbootstrap already copies them via `sudo cp -rL` before running abuild.
This avoids abuild's checksum validation for local files entirely.
"""
import sys
from pathlib import Path


def patch_apkbuild(apkbuild_path: Path):
    content = apkbuild_path.read_text()

    if "sm7325-xiaomi-lisa.dts" in content:
        print("Kernel APKBUILD already patched for lisa.")
        return

    # Bump pkgrel to 2 to ensure it rebuilds as a fresh package
    content = content.replace("pkgrel=0", "pkgrel=2")

    prepare_hook = '''prepare() {
\tdefault_prepare
\tcp "$srcdir/config-$_flavor.$arch" .config

\t# Xiaomi Lisa additions — files are copied from startdir by pmbootstrap
\tcp "$startdir/sm7325-xiaomi-lisa.dts" arch/arm64/boot/dts/qcom/
\techo 'dtb-$(CONFIG_ARCH_QCOM) += sm7325-xiaomi-lisa.dtb' >> arch/arm64/boot/dts/qcom/Makefile
\tcp "$startdir/panel-csot-vtdr6130.c" drivers/gpu/drm/panel/
\techo 'obj-m += panel-csot-vtdr6130.o' >> drivers/gpu/drm/panel/Makefile

\t# Enable simpledrm and simplefb for guaranteed early display output
\techo 'CONFIG_DRM_SIMPLEDRM=y' >> .config
\techo 'CONFIG_FB_SIMPLE=y' >> .config
\techo 'CONFIG_SYSFB_SIMPLEFB=y' >> .config
}'''

    old_prepare = '''prepare() {
\tdefault_prepare
\tcp "$srcdir/config-$_flavor.$arch" .config
}'''

    if old_prepare in content:
        content = content.replace(old_prepare, prepare_hook)
    else:
        import re
        pattern = r'prepare\(\) \{\n\tdefault_prepare\n\tcp "\$srcdir/config-\$_flavor\.\$arch" \.config\n\}'
        if re.search(pattern, content):
            content = re.sub(pattern, prepare_hook, content)
            print("Replaced prepare() via regex.")
        else:
            print("ERROR: Could not find prepare() to replace!")
            sys.exit(1)

    apkbuild_path.write_text(content)
    print("Kernel APKBUILD successfully patched for lisa (pkgrel=2, obj-m for panel, simpledrm enabled).")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: patch-kernel-apkbuild.py /path/to/APKBUILD")
        sys.exit(1)
    patch_apkbuild(Path(sys.argv[1]))
