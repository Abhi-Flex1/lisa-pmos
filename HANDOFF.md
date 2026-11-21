# Handoff — postmarketOS for Xiaomi 11 Lite 5G NE (`xiaomi-lisa`)

Status as of 2026-11-21. This repository is the working port tree.
The upstream source repo was `Abhi-Flex1/postmarket-lisa` (now deleted); this repository is the maintained port.

## TL;DR

* **Full postmarketOS (KDE Plasma Mobile + systemd-edge, Linux 7.2.9) is booted and running on real `xiaomi-lisa` hardware!**
  Live verification on the connected phone (`172.16.42.1`, PIN/password `147147`) confirmed:
  * **Display & Backlight**: Auto-wakes on boot straight into Plasma Mobile (`1080x2400` 90Hz/60Hz DSC OLED, `brightness=1500 actual_brightness=1500`, dynamic brightness working, OOBE completed)
  * **Touchscreen, Keys & PowerDevil**: `Goodix Berlin Capacitive TouchScreen`, power & volume keys (`POWERDEVIL_NO_DDCUTIL=1` fixes PowerDevil startup hang; `15 ms` debounce on Vol Up)
  * **Audio**: TFA9873 stereo speakers (`card 0: lisa`), `PulseAudio 17.0` with `tsched=0`, `48000 Hz`, and XWayland-free autostart drop-in (`99-pulseaudio-no-x11.conf`), fixing browser/YouTube audio & volume key deadlocks
  * **Haptics & Vibration**: Awinic AW8624 (`kernel/drivers/aw8624.c`, `i2c4 @ 0x5a`, chip ID `0x24`, `EV_FF`/`FF_RUMBLE`) integrated with `feedbackd` (`73-xiaomi-lisa-haptics.rules`)
  * **Camera**: Rear ultrawide 8MP Sony IMX355 (`sony,imx355` @ `cci1_i2c1 0x1a` + `qcom-camss` `port@3`) detected by `libcamera v0.8.0` with SoftISP `imx355.yaml` and exposed as `Built-in Back Camera` in PipeWire/Plasma Camera
  * **Wi-Fi & Bluetooth**: WCN6750 `wlan0` connected to Wi-Fi with internet access (`ping 1.1.1.1`), Bluetooth controller powers on & scans
  * **GPU, Sensors, Modem, Battery**: Adreno 642L (`GMU v3.1.10`), live accelerometer/ALS/compass (`iio-sensor-proxy`), `ModemManager` detects `qrtr0` + `rmnet_ipa0` (no SIM tested yet), `qcom-battmgr` battery & USB charging
  * **Remaining WIP**: Additional camera sensors (main 64MP, front 20MP, macro `s5k5e9`) and upstream SM7325 Iris video encoder reset caveat.

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

## Remaining follow-up work (optional / upstreaming)

1. Test cellular voice/data with a physical SIM card inserted (`ModemManager` already detects `qrtr0` + `rmnet_ipa0`).
2. Camera sensor bring-up (`pm8008` regulators + `s5k5e9` / main sensor binding in `sm7325-xiaomi-lisa.dts` + `libcamera` tuning).
3. Upstream `sm7325-xiaomi-lisa.dts`, `panel-csot-vtdr6130.c`, and the `xiaomi,lisa-sndcard` patch to `sc7280-mainline` / mainline Linux and open a pmaports MR.

## Reference material included

`reference/` contains `stock-boot-header.txt` and `stock-rom-info.md`.
The extracted stock `vbmeta.img`, `dtb.img` and `prog_firehose_lite.elf` are
kept **locally only** (gitignored) because this machine cannot push large
blobs reliably; extract them again from `lisa_stock.tgz` when needed.

The full 6.1 GB stock ROM archive is at
`/home/abhi/Desktop/lisa_stock/lisa_stock.tgz` on the original machine; it is
too large for GitHub but its contents are listed in `stock-rom-info.md`.
