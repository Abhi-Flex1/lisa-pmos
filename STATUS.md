# Port status — Xiaomi 11 Lite 5G NE (`xiaomi-lisa`)

This file is the honest, evidence-based status of the port. It distinguishes
**verified on the connected device**, **verified statically in the source
tree**, and **still requires a boot test on the device**.

## 1. Facts verified on the connected phone (2026-11-20)

Read-only ADB commands against serial `ebe7291`:

```
adb shell getprop ro.product.model                 -> 2109119DG
adb shell getprop ro.product.device                 -> lisa
adb shell getprop ro.boot.slot_suffix               -> _a
adb shell getprop ro.boot.dynamic_partitions        -> true
adb shell getprop ro.boot.flash.locked              -> 0        (bootloader UNLOCKED)
adb shell getprop ro.boot.verifiedbootstate         -> orange
adb shell ls -l /dev/block/by-name/super            -> /dev/block/sda20
adb shell ls /sys/class/drm/                        -> card0, card0-DSI-1, renderD128
adb shell cat /sys/class/drm/card0-DSI-1/status      -> connected
adb shell cat /proc/asound/cards                     -> 0 [lahainayupikqrd]: ...-snd-card
adb shell getevent -il                              -> name: "goodix_ts"
adb shell ls /dev/video*                            -> video0 video1 video32 video33
```

Consequences for the port:

* `deviceinfo_super_partitions="/dev/sda20 /dev/sda20"` is **correct** for this
  hardware (single super partition, virtual A/B).
* The device is **bootloader-unlocked** and in **orange** state, so flashing a
  custom `boot.img` is possible.
* The stock kernel exposes a real DRM/KMS card with a connected DSI panel and
  an Adreno render node. The mainline DTS in this repo carries the equivalent
  `&mdss_dsi` + `panel@0` (`csot,vtdr6130`) nodes, so Plasma Mobile's Wayland
  compositor has a DRM output. `deviceinfo_drm="true"` is therefore correct.
* Touchscreen is Goodix (`goodix,gt9897` in the DTS, `goodix_berlin_spi` in
  `modules-initfs`).

## 2. Verified statically in this repository

`./scripts/validate.sh` passes all checks:

```
VALIDATION: all checks passed
```

It verifies shell/Python syntax, every local APKBUILD sha512, required
`deviceinfo` fields, DTS panel/touch/soundcard/Wi-Fi nodes, that the soundcard
patch is actually applied by the kernel build, that Plasma Mobile is the
default UI, and that no unsupported `--ui` flag is passed to `pmbootstrap install`.

Additional static checks performed:

* The soundcard patch applies cleanly to the real
  `sc7280-mainline/linux` v7.2.9 `sound/soc/qcom/sm8250.c`
  (`patch --dry-run`: `Hunk #1 succeeded at 204`).
* The DTS was preprocessed and compiled against the **real v7.2.9 kernel
  headers** with `cpp` + `dtc`:
  ```
  preprocessed lines: 9837
  dtc exit: 0
  -rw-r--r-- 1 abhi abhi 146699 .../sm7325-xiaomi-lisa.dtb
  ```
  (Only `unique_unit_address` warnings from the shared `kodiak.dtsi` QUP
  i2c/spi/serial mux nodes, which are harmless and present upstream.)
* Every firmware manifest (`*.files`) sha512 matches the APKBUILD.
* The ALSA UCM2 profile is now shipped by a real aport
  (`audio/alsa-ucm-conf-lisa`) and resolved via
  `ucm2/conf.d/sm7325/lisa.conf` for the `snd-sm8250` card named `lisa`.
* `pmbootstrap status` with the generated config reports:

```
Channel: systemd-edge   Device: xiaomi-lisa (aarch64)   UI: plasma-mobile
```

## 3. Not yet verified — requires building + a boot test

The following cannot be claimed as "working" until a postmarketOS `boot.img`
and rootfs are built and booted on the device. This currently requires root
privileges on the build host (pmbootstrap creates chroots with `mknod`), which
are not available in this session.

| Subsystem | Implementation present | Status |
| :--- | :--- | :--- |
| Boot / initramfs | fastboot, appended DTB, dynamic-partition mapping | **unverified on device** |
| Display (DRM) | `msm` + `panel-csot-vtdr6130`, simplefb/simpledrm fallback | **unverified on device** |
| Touchscreen | `goodix_berlin_core/spi` in initfs | **unverified on device** |
| GPU | Adreno 642L (`firmware-qcom-adreno-a660`, Mesa Turnip) | **unverified on device** |
| Wi-Fi / BT | `wpss` firmware + `linux-firmware-qca` | **unverified on device** |
| Audio | soundcard compatible patch + new UCM2 profile | **unverified on device** |
| Modem / data | `soc-qcom-modem`, modem/IPA firmware, ModemManager via Plasma | **unverified on device** |
| Battery / charging | PMIC drivers in mainline config | **unverified on device** |
| Cameras | DTS sensor nodes, `vpu`/`sensors` firmware, `libcamera`/`plasma-camera` added | **WIP — sensor tuning needed** |
| Video encode (Venus) | upstream reports a hard reset on SM7325 encoder jobs | **known upstream limitation** |

### Known upstream limitations

* Venus **encoder** job execution hard-resets the SoC on SM7325 (decoder is
  fine). See the linux-arm-msm discussion:
  <https://ratatoskr.run/linux-arm-msm/2026/07/17283595/t>.
  This is outside this repository's packaging and cannot be fixed here.
* Camera ISP tuning for the S5K GW3 / S5K5E9 sensors is not upstreamed;
  expect manual `libcamera` tuning work.

## 4. What would make this "everything working"

1. Build the packages and image (`./scripts/build.sh plasma-mobile`) on a host
   with working sudo/root.
2. Non-destructively test with `./scripts/test-boot.sh` (`fastboot boot`),
   then SSH in and record `dmesg`, `ls /dev/dri`, `aplay -l`, `wifi` link,
   `modemmanager` state.
3. Fix anything that fails from that log, then flash with `./scripts/flash.sh`
   (only with explicit user approval — it overwrites `boot_a/b`, `userdata`).
4. Submit the DTS, panel driver and soundcard patch upstream, then open a
   pmaports MR for the three apports.

## 5. Boot-test outcome (2026-11-20, updated)

All artifacts were built successfully:

```
boot.img                       14M  (header v3, 4096B pages)
vendor_boot.img                14M
xiaomi-lisa.img               3.3G  (rootfs incl. postmarketos-ui-plasma-mobile)
linux-postmarketos-qcom-sc7280-7.2.9-r5.apk
device-xiaomi-lisa-1-r3.apk
firmware-xiaomi-lisa-*.apk (9 APKs, all 758 firmware files)
alsa-ucm-conf-lisa-1-r1.apk
```

`./scripts/validate.sh` passes; the DTS compiles against the real 7.2.9
kernel headers (`dtc exit: 0`, 146699-byte DTB).

**The device did not boot postmarketOS.** After flashing `boot_a/b`,
`vendor_boot_a/b`, `userdata` and a verification-disabled `vbmeta_a/b`, the
phone consistently returned to the bootloader. To isolate the cause, the
boot chain was replaced with official LineageOS 23.2 images for lisa
(`boot.img`, `vendor_boot.img`, `dtbo.img`, `vbmeta.img`) — **these also fail
to boot and return to fastboot**. `fastboot boot` is rejected by this
bootloader even for the official image
(`Failed to load/authenticate boot image: Load Error`).

Conclusion: the remaining failure is not in the port packages. It is in the
device's current bootloader/partition state and requires on-device recovery
(likely restoring a full stock firmware with MiFlash or flashing the complete
LineageOS build), plus a serial-console or recovery-based crash log to debug
the mainline kernel bring-up. I did not claim "everything working" because
that is not supported by the evidence.

## 6. Rebuild after full-lineageOS boot investigation (2026-11-20, evening)

The phone was restored by flashing a stock **HyperOS 2 fastboot ROM**
(`Xiaomi/lisa_global/lisa:14/UKQ1.240624.001/OS2.0.8.0.UKOMIXM`), and it
boots normally again. That confirms the earlier boot failures were not
hardware damage.

Findings that changed the plan:

* `deviceinfo_super_partitions` is consumed by the pmOS initramfs
  (`postmarketos-initramfs/init_functions.sh: setup_dynamic_partitions`),
  and `mount_subpartitions()` explicitly scans `/dev/disk/by-partlabel/userdata`
  for the two pmOS subpartitions. So flashing the generated rootfs image to
  `userdata` was the correct target.
* `pmbootstrap`'s own fastboot config flashes a vbmeta generated with
  **`--flags 2`** (`disable-verity`), not the flags=3 image used previously.
  `scripts/flash.sh` was updated to generate the correct vbmeta with
  `avbtool` before flashing.
* The ADB session to the stock system is authorized and available for
  pulling firmware/dtb/ucm data if needed.

The build workdir was lost during the EDL recovery attempts, so packages and
images are being rebuilt from the committed sources (`scripts/build.sh`).
Next step after rebuild: `scripts/flash.sh` with the corrected vbmeta, then
capture a live device report.

### Stock ROM comparison (2026-11-20)

Extracted from `/home/abhi/Desktop/lisa_stock/lisa_stock.tgz`
(HyperOS 2.0.8.0.UKOMIXM, Android 14):

* `boot.img`, `boot-gki.img`, `vendor_boot.img`, `vbmeta.img`, `dtb.img`,
  `dtbo.img`, `super.img`, `prog_firehose_ddr.elf`, `prog_firehose_lite.elf`
  are all available in the archive.
* Stock boot header version is **3** (header bytes at 0x28 = `03 00 00 00`),
  matching pmbootstrap's generated boot.img. So boot-header format was not
  the cause of the earlier failure.
* Stock `vbmeta.img` has `Flags: 0` and AVB descriptors for boot/vendor_boot;
  postmarketOS must flash a vbmeta with `--flags 2` (disable-verity) instead
  of the flags=3 image used in earlier attempts. `scripts/flash.sh` now does
  exactly that.
