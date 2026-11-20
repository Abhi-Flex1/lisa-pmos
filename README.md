# postmarketOS for Xiaomi 11 Lite 5G NE (`xiaomi-lisa`)

Mainline postmarketOS port for the Xiaomi 11 Lite 5G NE / 11 LE
(codenames `lisa`, models `2107119DG` / `2107119DC`, Snapdragon 778G / SM7325).

**Default user interface: KDE Plasma Mobile.**

> Read [STATUS.md](STATUS.md) for the evidence-based feature status. In short:
> the packaging is complete and statically validated; a full image build and
> on-device boot test are still pending (the build host needs root privileges
> for pmbootstrap chroots).

## What is in this repository

```
device/device-xiaomi-lisa/     device package (deviceinfo, initfs modules)
firmware/firmware-xiaomi-lisa/ proprietary adsp/cdsp/gpu/ipa/modem/... firmware
audio/alsa-ucm-conf-lisa/      ALSA UCM2 profiles for the lisa soundcard
kernel/dts/                    sm7325-xiaomi-lisa.dts (panel, touch, audio, BT)
kernel/drivers/                CSOT VTDR6130 DRM panel driver
kernel/patches/                snd-sm8250 compatible-string quirk
scripts/                       build / validate / test-boot / flash helpers
docker/                        Alpine + pmbootstrap container (for macOS)
pmaports/                      local pmaports checkout used by build.sh
```

## Quick start

```bash
./scripts/validate.sh                 # static checks (no root needed)
./scripts/build.sh plasma-mobile      # default; builds image into output/images
./scripts/test-boot.sh                # non-destructive: fastboot boot output/images/boot.img
```

`plasma-mobile` is the default UI. You can pass another UI name, but Plasma
Mobile is the one this port targets and validates.

See [INSTALL.md](INSTALL.md) for prerequisites, detailed steps and flashing.

## Verified device facts

* Bootloader unlocked (`ro.boot.flash.locked=0`, orange state).
* Active slot `_a`; virtual A/B with a single `super` at `/dev/block/sda20`,
  matching `deviceinfo_super_partitions="/dev/sda20 /dev/sda20"`.
* DRM card `card0` with connected `DSI-1` and `renderD128` on the stock OS —
  the mainline DTS provides the equivalent output for Wayland/Plasma Mobile.

## Port highlights

* Kernel: `linux-postmarketos-qcom-sc7280` (mainline 7.2 fork for
  SC7280/SM7325/QCM6490), with the lisa DTB, panel module, soundcard quirk and
  simplefb/simpledrm fallback injected by `scripts/patch-kernel-apkbuild.py`.
* Display: CSOT VTDR6130 1080x2400 DSI panel (DRM) + simpledrm fallback.
* Touch: Goodix GT9897 (`goodix_berlin_spi`).
* Audio: PRI_MI2S to dual NXP TFA9873 amps, with a shipped UCM2 profile.
* Firmware: packaged from
  [mainlining/firmware-xiaomi-lisa](https://github.com/mainlining/firmware-xiaomi-lisa).
* UI: `postmarketos-ui-plasma-mobile` (systemd channel), plus
  `plasma-camera` and `libcamera` for the camera stack.

## Credits

* Jens Reidel (`Aelin`) — initial device tree, panel and touchscreen bringup.
* Stanislav Zaikin — SM7325 (`xiaomi-taoyao`) reference and audio bringup.
* Luca Weiss (`z3ntu`) — `sc7280-mainline` kernel maintenance.
* postmarketOS community — pmaports and tooling.

Licensed under the terms in [LICENSE](LICENSE).
