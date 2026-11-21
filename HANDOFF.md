# Handoff — postmarketOS for Xiaomi 11 Lite 5G NE (`xiaomi-lisa`)

Status as of 2026-11-20 (late night). This repository is the working port tree.
It was forked from `Abhi-Flex1/postmarket-lisa` and contains all work done since.

## TL;DR

* **Mainline Linux 7.2.9 (`linux-postmarketos-qcom-sc7280`) boots on real `xiaomi-lisa` hardware!**
  Live verification over USB (`18d1:d001`, `172.16.42.1` CDC NCM + `/dev/ttyACM0`) confirmed:
  * **Display**: `msm` DPU (`0x70020000`) + `panel-csot-vtdr6130` (`card1-DSI-1: connected`, `renderD128`, `fb0: msmdrmfb`, `buffyboard` active)
  * **Touchscreen**: `Goodix Berlin Capacitive TouchScreen` (`goodix_berlin_spi`)
  * **Buttons & UFS**: `pmic_pwrkey`, `pmic_resin`, `gpio-keys`, `super -> sda20`, `userdata -> sda33`
* **Fresh Plasma Mobile images are built in `output/images/`** (`boot.img`, `dtbo.img`, `xiaomi-lisa.img`) with all discovered fixes included (`header_version=0`, `append_dtb=true`, `base=0x00000000`, `earlycon`, zeroed `dtbo.img`, `linux-firmware-ath11k`, `hexagonrpcd`, `mkbootimg`, fixed UCM2).
* Next step when physically at the phone: hold **Power + Volume Down** to enter fastboot and run `./scripts/flash.sh`, then verify userspace subsystems with `./scripts/device-report.sh ssh`.

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

## Critical technical findings (verified in stock `abl.elf` and on hardware)

1. **Bootloader (`LinuxLoader.efi`) — SOLVED**:
   * **Zero `dtbo_a/b`**: When `dtbo` has valid `0xd7b7ab1e` magic, ABL's `GetDtboNeeded()` returns true and forces `ufdt_apply_overlay()` + Gunyah `hyp` overlays, which fail on mainline DTBs (`Reason:BootLinux Fail`). Flashing a zeroed `dtbo.img` bypasses all DTBO/hyp overlays.
   * **Use `header_version="0"` + `append_dtb="true"` + `base="0x00000000"` + `earlycon`**: In the no-DTBO fallback path (`Single appended DTB found`), ABL adds `+page_size` to `DtbOffset` — which matches header v0's appended DTB and breaks header v2/v3. `vendor_boot` is not used at all in header v0 mode.
   * **Fastboot diagnostics**: `fastboot oem fbreason` reports the exact boot failure reason; `fastboot oem lkmsg` dumps the 2 MB `console-ramoops` buffer via raw USB `DATA` upload.
2. **vbmeta**: stock `vbmeta.img` has `Flags: 0` and AVB descriptors.
   postmarketOS flashes a vbmeta generated with `--flags 2` (`disable-verification`).
   `scripts/flash.sh` generates this automatically with `avbtool`.
3. **Rootfs target**: `deviceinfo_super_partitions="/dev/sda20 /dev/sda20"`
   is consumed by pmOS initramfs (`setup_dynamic_partitions`), and
   `mount_subpartitions()` scans `/dev/disk/by-partlabel/userdata` (`/dev/sda33`).
   Flashing `output/images/xiaomi-lisa.img` to `userdata` is verified.
4. **ADB / Fastboot / EDL**: serial `ebe7291`; bootloader unlocked (`unlocked: yes`); active slot `a`.
   EDL works with the stock HyperOS firehose loader (`prog_firehose_lite.elf`).
5. **Panel & Touch (verified live on 7.2.9)**: CSOT VTDR6130, 1080x2400 (`card1-DSI-1`, `renderD128`, `fb0: msmdrmfb`).
   Touch: Goodix GT9897 (`goodix_berlin_spi`, `Goodix Berlin Capacitive TouchScreen`).
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

## What remains to finish full userspace bring-up

1. Put the phone in fastboot (Power + Volume Down) and run `./scripts/flash.sh`
   (flashes zeroed `dtbo_a/b`, disabled `vbmeta_a/b`, header-v0 `boot_a/b`, and
   the Plasma Mobile rootfs `output/images/xiaomi-lisa.img` to `userdata`).
2. After reboot into Plasma Mobile, verify USB networking / SSH (`ssh user@172.16.42.1`,
   password `147147`) and run `./scripts/device-report.sh ssh`.
3. Verify each userspace subsystem (Plasma Mobile Wayland session, Wi-Fi via `ath11k`,
   Bluetooth, TFA9873 speaker audio + UCM2, ModemManager, battery/charging, camera)
   and update `STATUS.md`.

## Reference material included

`reference/` contains `stock-boot-header.txt` and `stock-rom-info.md`.
The extracted stock `vbmeta.img`, `dtb.img` and `prog_firehose_lite.elf` are
kept **locally only** (gitignored) because this machine cannot push large
blobs reliably; extract them again from `lisa_stock.tgz` when needed.

The full 6.1 GB stock ROM archive is at
`/home/abhi/Desktop/lisa_stock/lisa_stock.tgz` on the original machine; it is
too large for GitHub but its contents are listed in `stock-rom-info.md`.
