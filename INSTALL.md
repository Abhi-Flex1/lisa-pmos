# Install postmarketOS (Plasma Mobile) on `xiaomi-lisa`

## 0. Prerequisites

* Xiaomi 11 Lite 5G NE (`lisa`). The connected test device reports:
  * model `2109119DG`, active slot `_a`
  * `ro.boot.flash.locked=0` and `verifiedbootstate=orange` (bootloader unlocked)
  * single dynamic `super` at `/dev/block/sda20`
* A Linux host with `pmbootstrap >= 3.12`, `git`, `adb`, `fastboot` and
  **working sudo/root** (pmbootstrap creates chroots using `mknod`; a plain
  unprivileged user cannot do this).
  * On macOS use the provided Docker/Colima wrapper:
    `./docker/run-pmbootstrap.sh /workspace/postmarket-lisa/scripts/build.sh plasma-mobile`
* USB debugging enabled for the initial read-only checks.

If the bootloader is locked, unlock it first on the device:
Settings → About phone → Tap "MIUI version" 7 times → Additional settings →
Developer options → "OEM unlocking", then reboot to fastboot and run
`fastboot flashing unlock`. **This wipes userdata.**

## 1. Validate the port

```bash
./scripts/validate.sh
```

Expected: `VALIDATION: all checks passed`.

## 2. Build the image

```bash
./scripts/build.sh plasma-mobile
```

This will:

1. stage `device-xiaomi-lisa`, `firmware-xiaomi-lisa` and
   `alsa-ucm-conf-lisa` into `pmaports/device/testing/`;
2. inject the lisa DTS, panel driver and soundcard patch into
   `linux-postmarketos-qcom-sc7280`;
3. write a deterministic pmbootstrap config (`device=xiaomi-lisa`,
   `ui=plasma-mobile`);
4. build the apports, kernel, rootfs and `boot.img` into `output/images/`.

The kernel build is the slow step; subsequent runs reuse cached APKs.

## 3. Non-destructive boot test (do this first)

```bash
./scripts/test-boot.sh
```

This reboots the phone to the bootloader and runs `fastboot boot
output/images/boot.img`. Nothing is written to internal storage. If the image
boots, USB networking comes up:

```bash
ssh user@172.16.42.1     # password: 147147 (from build.sh)
```

On the phone, capture evidence:

```bash
dmesg | tail -200
cat /proc/asound/cards
ls /dev/dri/
ip link
modem-manager --version 2>/dev/null || mmcli -L
```

## 4. Flash permanently

Only after a successful test boot, and only if you accept that this overwrites
`boot_a`, `boot_b` and `userdata`:

```bash
./scripts/flash.sh
```

The script asks for confirmation and flashes:

* `boot_a` / `boot_b` with the built `boot.img`
* `vendor_boot_a` / `vendor_boot_b` if a `vendor_boot.img` was exported
* `userdata` with the generated rootfs image
* if you are recovering from a broken stock install,
  `./scripts/flash_and_boot_recovery.sh` restores known-good LineageOS
  vbmeta/dtbo/vendor_boot/boot images from `output/images/` (you must supply
  those files yourself).

## 5. First boot into Plasma Mobile

* The UI is `postmarketos-ui-plasma-mobile` (systemd channel).
* Wi-Fi: configure via Plasma Mobile settings (firmware is preinstalled).
* Cellular data: ModemManager is enabled by the UI; check with `mmcli -L`.
* Audio: the `lisa` UCM2 profile is installed at
  `/usr/share/alsa/ucm2/Xiaomi/lisa/` and registered in
  `ucm2/conf.d/sm7325/`.
* Camera: `libcamera` and `plasma-camera` are installed; sensor tuning is still
  work in progress (see STATUS.md).

## Troubleshooting

* No display: confirm the kernel contains `panel-csot-vtdr6130.ko` and that
  `modules-initfs` lists it; the simplefb fallback should still give a console.
* No touch: check `goodix_berlin_core` / `goodix_berlin_spi` in initfs.
* No audio: verify `aplay -l` shows the `lisa` card and that the UCM symlink
  exists; the soundcard patch must be applied in the kernel build.
* `pmbootstrap` fails with `mknod ... Operation not permitted`: you need real
  root/sudo; user namespaces are not sufficient.
