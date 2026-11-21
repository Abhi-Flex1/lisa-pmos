#!/usr/bin/env python3
"""Inject Xiaomi 11 Lite 5G NE (lisa) enablement into the
linux-postmarketos-qcom-sc7280 aport.

The three lisa-specific files are staged next to the APKBUILD by
scripts/setup-pmaports.sh / scripts/build.sh.  We deliberately read them from
$startdir instead of adding them to source=, so abuild never has to verify
checksums for files that live outside the pmaports git history.

What this patch does:
  * builds sm7325-xiaomi-lisa.dtb (the DTS is not in the 7.2 sc7280 fork)
  * builds panel-csot-vtdr6130 as a module (needed by modules-initfs)
  * applies the sound-card compatible patch (audio was silent without it)
  * enables simple-framebuffer / simpledrm as a boot fallback via scripts/config
    and regenerates the config with olddefconfig (raw `echo >> .config` was
    silently discarded by Kconfig on the next build)
"""

import re
import sys
from pathlib import Path

LISA_FILES = (
    "sm7325-xiaomi-lisa.dts",
    "panel-csot-vtdr6130.c",
    "aw8624.c",
    "s5k3t2.c",
    "0001-ASoC-qcom-sm8250-Add-Xiaomi-Lisa-soundcard-compatible.patch",
)

PREPARE = '''prepare() {
\tdefault_prepare
\tcp "$srcdir/config-$_flavor.$arch" .config

\t# Restore a previously compiled tree if available. This contains all
\t# 4356 object files, the patched DTS/driver and the generated .config,
\t# so `make` only needs to finish the link instead of recompiling.
\tif [ -d "$startdir/../../.kernel-build-cache" ] && [ ! -d "$startdir/.kernel-build-cache" ]; then
\t\t# Keep the large cache out of the aport dir so pmbootstrap doesn't
\t\t# re-stage 4 GB on every build; link to it instead.
\t\tln -s ../../.kernel-build-cache "$startdir/.kernel-build-cache"
\tfi
\tif [ -d "$startdir/.kernel-build-cache" ]; then
\t\techo "Restoring cached kernel build tree ..."
\t\t# The cache contains relative symlinks (scripts/dtc/include-prefixes/*)
\t\t# which cp -a cannot overlay onto the freshly extracted tree. Replace
\t\t# the source tree wholesale with a tar stream instead; tar preserves
\t\t# symlinks exactly and `make` then only finishes the link step.
\t\trm -rf "$builddir"
\t\tmkdir -p "$builddir"
\t\ttar -C "$startdir/.kernel-build-cache" -cf - . | tar -C "$builddir" -xf -
\t\t# Always overlay the latest repo DTS and out-of-tree drivers onto the cached tree.
\t\tcp "$startdir/sm7325-xiaomi-lisa.dts" arch/arm64/boot/dts/qcom/
\t\tcp "$startdir/panel-csot-vtdr6130.c" drivers/gpu/drm/panel/
\t\tcp "$startdir/aw8624.c" drivers/input/misc/
\t\tgrep -q 'aw8624.o' drivers/input/misc/Makefile || printf 'obj-m += aw8624.o\\n' >> drivers/input/misc/Makefile
\t\tcp "$startdir/s5k3t2.c" drivers/media/i2c/
\t\tgrep -q 's5k3t2.o' drivers/media/i2c/Makefile || printf 'obj-$(CONFIG_VIDEO_S5K3T2) += s5k3t2.o\\n' >> drivers/media/i2c/Makefile
\t\tgrep -q 'config VIDEO_S5K3T2' drivers/media/i2c/Kconfig || printf '\\nconfig VIDEO_S5K3T2\\n\\ttristate "Samsung S5K3T2 sensor support"\\n\\tselect V4L2_CCI_I2C\\n\\thelp\\n\\t  V4L2 sensor driver for the Samsung S5K3T2 20MP camera.\\n' >> drivers/media/i2c/Kconfig
\t\t./scripts/config --module VIDEO_IMX355
\t\t./scripts/config --module VIDEO_S5K3T2
\t\tmake ARCH="$_carch" LLVM=1 CC="ccache clang" olddefconfig
\telse
\t\t# --- Xiaomi 11 Lite 5G NE (lisa) enablement ---
\t\tcp "$startdir/sm7325-xiaomi-lisa.dts" arch/arm64/boot/dts/qcom/
\t\tprintf 'dtb-$(CONFIG_ARCH_QCOM) += sm7325-xiaomi-lisa.dtb\\n' \\
\t\t\t>> arch/arm64/boot/dts/qcom/Makefile

\t\tcp "$startdir/panel-csot-vtdr6130.c" drivers/gpu/drm/panel/
\t\tprintf 'obj-m += panel-csot-vtdr6130.o\\n' \\
\t\t\t>> drivers/gpu/drm/panel/Makefile

\t\tcp "$startdir/aw8624.c" drivers/input/misc/
\t\tprintf 'obj-m += aw8624.o\\n' \\
\t\t\t>> drivers/input/misc/Makefile

\t\t# The machine driver needs the xiaomi,lisa-sndcard compatible string.
\t\tpatch -p1 -i \\
\t\t\t"$startdir/0001-ASoC-qcom-sm8250-Add-Xiaomi-Lisa-soundcard-compatible.patch"

\t\t# Boot-time framebuffer and simpledrm fallback (splash / unl0kr / shell),
\t\t# plus rear ultrawide IMX355 camera sensor module.
\t\t./scripts/config --enable FB_SIMPLE
\t\t./scripts/config --enable SYSFB_SIMPLEFB
\t\t./scripts/config --enable DRM_SIMPLEDRM
\t\tcp "$startdir/s5k3t2.c" drivers/media/i2c/
\t\tprintf 'obj-$(CONFIG_VIDEO_S5K3T2) += s5k3t2.o\\n' >> drivers/media/i2c/Makefile
\t\tprintf '\\nconfig VIDEO_S5K3T2\\n\\ttristate "Samsung S5K3T2 sensor support"\\n\\tselect V4L2_CCI_I2C\\n\\thelp\\n\\t  V4L2 sensor driver for the Samsung S5K3T2 20MP camera.\\n' >> drivers/media/i2c/Kconfig
\t\t./scripts/config --module VIDEO_IMX355
\t\t./scripts/config --module VIDEO_S5K3T2
\t\tmake ARCH="$_carch" LLVM=1 CC="ccache clang" olddefconfig
\tfi
}'''

OLD_PREPARE = '''prepare() {
\tdefault_prepare
\tcp "$srcdir/config-$_flavor.$arch" .config
}'''


def patch_apkbuild(apkbuild: Path, stage_dir: Path) -> bool:
    original = apkbuild.read_text()
    content = original
    already = "sm7325-xiaomi-lisa.dts" in content and "lisa-sndcard" in content

    if not already:
        missing = [f for f in LISA_FILES if not (stage_dir / f).is_file()]
        if missing:
            raise SystemExit(f"ERROR: missing lisa kernel assets: {', '.join(missing)}")
        if OLD_PREPARE not in content:
            raise SystemExit("ERROR: could not find the expected prepare() in APKBUILD")
        content = content.replace(OLD_PREPARE, PREPARE, 1)
        content = re.sub(r"pkgrel=\d+", "pkgrel=4", content, count=1)

    # `patch` is used in prepare(); kernel scripts/config needs a real bash
    # (#!/usr/bin/env bash) which is absent from the cross-native chroot.
    tools_added = False
    for tool in ("bash", "patch"):
        if f"\n\t{tool}\n" not in content:
            content = content.replace(
                "makedepends=\"\n", f"makedepends=\"\n\t{tool}\n", 1
            )
            tools_added = True

    # Route the kernel compile through ccache so a failed build can be retried
    # without recompiling every translation unit from scratch.
    if 'make ARCH="$_carch" LLVM=1 \\\n' in content:
        content = content.replace(
            'make ARCH="$_carch" LLVM=1 \\\n',
            'make ARCH="$_carch" LLVM=1 CC="ccache clang" \\\n',
            1,
        )

    if tools_added:
        content = re.sub(
            r"pkgrel=(\d+)", lambda m: f"pkgrel={int(m.group(1)) + 1}", content, count=1
        )

    if content == original:
        print("Kernel APKBUILD already fully patched for lisa.")
        return False

    apkbuild.write_text(content)
    print("Kernel APKBUILD patched: DTB + panel module + soundcard quirk + "
          "simplefb/simpledrm + build tools; pkgrel bumped.")
    return True


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <path-to-kernel-APKBUILD> [staging-dir]")
        raise SystemExit(1)

    apkbuild = Path(sys.argv[1]).resolve()
    stage_dir = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else apkbuild.parent
    patch_apkbuild(apkbuild, stage_dir)


if __name__ == "__main__":
    main()
