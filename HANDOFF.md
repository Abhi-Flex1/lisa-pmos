# Handoff — postmarketOS for Xiaomi 11 Lite 5G NE (`xiaomi-lisa`)

Status as of 2026-11-20 (late night). This repository is the working port tree.
It was forked from `Abhi-Flex1/postmarket-lisa` and contains all work done since.

## TL;DR

* The **software port is complete and statically validated** (`./scripts/validate.sh`
  passes). The device packages, firmware, UCM audio profile, kernel integration,
  Plasma Mobile UI config and build scripts are all here.
* A full kernel + rootfs build was completed successfully earlier; images were
  produced, but the phone **did not boot postmarketOS**. The phone was later
  restored by the user by flashing stock HyperOS 2 fastboot ROM
  (`OS2.0.8.0.UKOMIXM`, Android 14) and now boots normally.
* A rebuild is in progress (`build2.log`) after the build workdir was lost during
  EDL recovery attempts. If you pick this up, check `build2.log` first.

## Repository layout

```
device/device-xiaomi-lisa/     device package (deviceinfo, initfs modules)
firmware/firmware-xiaomi-lisa/ 758 proprietary firmware blobs (manifests)
audio/alsa-ucm-conf-lisa/      ALSA UCM2 profile for the lisa soundcard
kernel/dts/                    sm7325-xiaomi-lisa.dts
kernel/drivers/                CSOT VTDR6130 DRM panel driver
kernel/patches/                snd-sm8250 lisa soundcard compatible patch
scripts/build.sh               full build (device, firmware, kernel, rootfs, export)
scripts/flash.sh               fastboot flashing with correct vbmeta flags
scripts/patch-kernel-apkbuild.py   inject DTS/panel/patch into kernel aport
scripts/patch-apk-usrmerge.sh  workaround for Alpine edge apk usrmerge bug
scripts/patch-pmbootstrap.sh   workaround for same bug in pmbootstrap itself
scripts/validate.sh            static validation (passes)
scripts/device-report.sh       read-only diagnostics from booted device
reference/                     stock ROM facts + small extracted reference images
STATUS.md                      detailed evidence-based status log
```

## Critical technical findings (do not redo this investigation)

1. **Bootloader**: Xiaomi lisa uses GKI boot images (`boot` + `vendor_boot`).
   Stock boot header version is **3** (verified in `reference/stock-boot-header.txt`),
   matching pmbootstrap. Boot format was never the failure.
2. **vbmeta**: stock `vbmeta.img` has `Flags: 0` and AVB descriptors.
   postmarketOS must flash a vbmeta generated with `--flags 2` (disable-verity).
   Earlier attempts used `--flags 3` and likely failed for that reason.
   `scripts/flash.sh` now generates flags=2 vbmeta with `avbtool`.
3. **Rootfs target**: `deviceinfo_super_partitions="/dev/sda20 /dev/sda20"`
   is consumed by pmOS initramfs (`setup_dynamic_partitions`), and
   `mount_subpartitions()` scans `/dev/disk/by-partlabel/userdata`.
   Flashing the generated rootfs to `userdata` is correct.
4. **ADB**: serial `ebe7291`; bootloader unlocked (`unlocked: yes`); active slot `a`.
   Stock build fingerprint at time of testing:
   `Xiaomi/lisa_global/lisa:14/UKQ1.240624.001/OS2.0.8.0.UKOMIXM:user/release-keys`.
5. **Panel**: CSOT VTDR6130, 1080x2400; DRM card `card0-DSI-1`, `renderD128`.
   Touch: Goodix GT9897 (`goodix_berlin_spi`).
6. **Upstream bug (workaround included)**: Alpine edge apk-tools 3.0.10 reports
   benign "failed to commit ... No such file or directory" errors for duplicate
   `/bin` vs `/usr/bin` paths. Scripts patch pmbootstrap/abuild wrappers to
   tolerate those errors; see `scripts/patch-apk-usrmerge.sh`.
7. **Build environment**: pmbootstrap 3.12.1, workdir `.pmos_cache` (gitignored).
   On a fresh machine run `./scripts/build.sh plasma-mobile`. If a build fails
   with `Permission denied` writing to `/home/pmos/packages/...`, run:
   `sudo chown -R 12345:12345 .pmos_cache/packages` and retry.
8. **Disk quota**: this host has a ~100 GB per-user quota; large ROM/EDL
   downloads caused "Disk quota exceeded" build failures. Keep temp files small.

## What remains to make it actually boot

1. Finish the rebuild (`build2.log`) on a machine with enough disk quota.
2. Put the phone in fastboot, run `./scripts/flash.sh` (asks for confirmation).
3. If it still returns to fastboot, capture a real kernel log. `fastboot boot`
   is rejected by this bootloader even for official images, so use flashed boots
   plus either:
   * a serial/UART console, or
   * boot a working recovery and read `/sys/fs/pstore/console-ramoops*`.
4. Fix whatever the crash log shows. Likely candidates: panel probe order,
   `simplefb`/DRM conflict, or initramfs waiting for a partition.
5. Verify each subsystem with `./scripts/device-report.sh ssh` and update STATUS.md.

## Reference material included

`reference/` contains small extracted files from the exact stock ROM the user
flashed (`lisa_stock.tgz`, HyperOS 2.0.8.0.UKOMIXM):
* `vbmeta.img` — stock AVB metadata (8 KB)
* `dtb.img` — stock DTB blob (4.1 MB)
* `prog_firehose_lite.elf` — stock EDL firehose loader (929 KB)
* `stock-boot-header.txt` — parsed header bytes showing header version 3
* `stock-rom-info.md` — full stock ROM facts

The full 6.1 GB stock ROM archive is at
`/home/abhi/Desktop/lisa_stock/lisa_stock.tgz` on the original machine; it is
too large for GitHub but its contents are listed in `stock-rom-info.md`.
