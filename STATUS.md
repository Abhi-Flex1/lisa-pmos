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
## 7. Mainline Linux 7.2.9 boot achieved on hardware (2026-11-21)

### Root cause of previous `Reason:BootLinux Fail` loops (from stock `abl.elf` / `LinuxLoader.efi`)

Decompressing and disassembling the stock HyperOS `abl.elf` (`LinuxLoader.efi`) plus querying `fastboot oem fbreason` revealed the exact two blockers:

1. **DTBO / Gunyah `hyp` overlay failure (`0x2a898` -> `0x171f4`)**:
   Whenever `dtbo_a/b` starts with the valid DTBO magic (`0xd7b7ab1e`), `GetDtboNeeded()` returns true and `BootLinux()` unconditionally runs `ufdt_apply_overlay()` (including Gunyah `hyp` overlays), which fails on a mainline DTB without downstream `__symbols__`, returning `Reason:BootLinux Fail`. Zeroing `dtbo_a/b` causes `GetDtboNeeded()` to return false and bypass all DTBO/hyp overlays.
2. **Header v2 vs Header v0 DTB offset (`0x17028` + `0x1740c`)**:
   In the no-DTBO fallback path (`0x173f4` -> `0x17850`, `"Single appended DTB found"`), ABL computes `boot_img_buf + page_size + DtbOffset`. For header v2, `DtbOffset` already includes `+page_size` so ABL overshoots by 4096 bytes. For **header v0** (`deviceinfo_header_version="0"`) with **`deviceinfo_append_dtb="true"`**, **`deviceinfo_flash_offset_base="0x00000000"`** (matching stock `vendor_boot` offsets `kernel=0x8000`, `ramdisk=0x1000000`, `tags=0x100`), and **`earlycon`**, ABL hits `"Single appended DTB found"` and boots mainline Linux cleanly.

### Live hardware verification from booted mainline kernel (`7.2.9 #6-postmarketos-qcom-sc7280`)

Captured directly from the running postmarketOS initramfs shell over USB (`/dev/ttyACM0`, USB gadget `18d1:d001`, `Product: Xiaomi 11 Lite 5G NE / 11 LE`, `SerialNumber: postmarketOS`):

```
uname -a                                -> Linux (none) 7.2.9 #6-postmarketos-qcom-sc7280 SMP PREEMPT aarch64
USB networking (cdc_ncm)                -> 172.16.42.1 up (ping RTT ~2.0 ms, DHCP assigns 172.16.42.2)
/dev/dri/                               -> card1, renderD128
/sys/class/drm/card1-DSI-1/status       -> connected
dmesg (DRM/panel)                       -> dpu hardware revision:0x70020000; Initialized msm 1.13.0; fb0: msmdrmfb
/sys/class/input/event*/device/name     -> pmic_pwrkey, pmic_resin, gpio-keys, Goodix Berlin Capacitive TouchScreen, buffyboard
/dev/disk/by-partlabel/super            -> ../../sda20
/dev/disk/by-partlabel/userdata         -> ../../sda33
```

### Rootfs/packaging fixes applied after live audit

Before generating the final Plasma Mobile images (`output/images/`):

* Updated `device/device-xiaomi-lisa/deviceinfo` to the hardware-verified boot config (`header_version="0"`, `append_dtb="true"`, `base="0x00000000"`, `earlycon console=tty0`, `dtbo` + `vbmeta` + `sparse`).
* Added missing SM7325 runtime dependencies to `device/device-xiaomi-lisa/APKBUILD`: `linux-firmware-ath11k` (WCN6750 Wi-Fi firmware, previously absent from `/lib/firmware/`), `hexagonrpcd>=0.3.2-r3` (ADSP/CDSP/sensors FastRPC), `mkbootimg` (on-device `mkinitfs` updates), and packaged a zeroed `/boot/dtbo.img`.
* Removed the duplicate conflicting `Earpiece` entry on `hw:${CardId},0` from `audio/alsa-ucm-conf-lisa/HiFi.conf`.
* Updated `scripts/flash.sh` to flash zeroed `dtbo_a/b`, disabled `vbmeta_a/b`, header-v0 `boot_a/b`, and `userdata`, and fixed `scripts/build.sh` directory cleanup (`rm -rf "$IMAGES_DIR"/*`).

### Remaining on-device step (COMPLETED — see Section 8 below)

Flashing and full live Plasma Mobile userspace bring-up were completed on real hardware (`xiaomi-lisa`, serial `ebe7291`).

## 8. Full KDE Plasma Mobile live bring-up & hardware verification (2026-11-21)

Flashed the Plasma Mobile rootfs + boot image to internal UFS and debugged + verified every major subsystem directly on the connected phone (`Linux xiaomi-lisa 7.2.9 #6-postmarketos-qcom-sc7280 SMP PREEMPT aarch64`):

### Fixes made and verified on real hardware

1. **Display auto-wake, 90Hz/60Hz timings, OLED regulator voltages & backlight (`kernel/drivers/panel-csot-vtdr6130.c`, `kernel/dts/sm7325-xiaomi-lisa.dts`)**:
   * Decompiled stock HyperOS `dtbo.img` (entry 7, `qcom,mdss_dsi_k9d_36_02_0a_dsc_cmd`) and verified this phone has the Tianma `k9d_36_02_0a` 1080x2400 DSC command-mode OLED panel (already matching the init sequence in `panel-csot-vtdr6130.c`, kept under compatible `csot,vtdr6130`).
   * Fixed 90Hz preferred mode timings (`vfp=4`, `clock=241793 kHz`, `0xb2 0x12 0x12`, `DRM_MODE_TYPE_PREFERRED`) — previously 90Hz erroneously used 60Hz's `vfp=1212` (`362689 kHz`) and `0xb2 0x0c 0x0c`.
   * Fixed browned-out OLED/DSI regulator bounds in `sm7325-xiaomi-lisa.dts`: `vreg_l13c_3p0` (`3.0V`, was `2.704V`), `vreg_l12c_1p8` (`1.8V`, was `1.656V`), `vreg_l10c_0p88` (`0.88V`, was `0.720V`), and `vreg_l6b_1p2` (`1.2V`, was `1.144V`).
   * Fixed 4-byte `0x51 <hi> <lo> 0x00 0x00` DCS backlight writes in HS mode and moved panel initialization + DSC PPS into `.enable()` with 150ms post-reset settle and `MIPI_DCS_GET_POWER_MODE` (`0x0a`) wake verification.
   * **Verified live after cold reboot**: screen lights up automatically straight into KDE Plasma Mobile (`brightness=1500 actual_brightness=1500 max=4095`), full OOBE completed, dynamic brightness changes latch cleanly on vsync.
2. **Audio — kernel Q6AFE/LPASS routing & PulseAudio UCM2 deadlock (`kernel/dts/sm7325-xiaomi-lisa.dts`, `audio/alsa-ucm-conf-lisa/HiFi.conf`)**:
   * Added `&q6afedai { dai@16 { reg = <PRIMARY_MI2S_RX>; qcom,sd-lines = <1>; }; };` (fixed `ASoC error (-22) on PRI_MI2S_RX: no line is assigned`) and `&lpass_audiocc { compatible = "qcom,qcm6490-lpassaudiocc"; /delete-property/ power-domains; };` (fixed `lpass_audio_cc-sc7280 probe failed -110`).
   * Removed `PlaybackMixer "default:${CardId}"` from `audio/alsa-ucm-conf-lisa/HiFi.conf`, which had caused `pulseaudio` to deadlock against `alsa-plugins-pulse` (`unix_wait_for_peer`) on its own socket.
   * **Verified live after cold reboot**: both TFA9873 stereo amps (`3-0034`, `3-0035`, rev `0x0b73`) probe cleanly, zero Q6AFE/LPASS/ASoC errors, `PulseAudio 17.0` auto-starts with `Default Sink: alsa_output.platform-sound.HiFi__Speaker__sink` (`[Out] Speaker`), and audible playback confirmed both via ALSA (`speaker-test -D plughw:0,0`) and PulseAudio (`speaker-test -D pulse`).

### Live hardware verification matrix (2026-11-21)

| Subsystem | Live evidence on connected `xiaomi-lisa` | Status |
| :--- | :--- | :--- |
| Boot & UFS storage | Boots directly from internal UFS (`boot_a/b` header v0 + appended DTB, zeroed `dtbo_a/b`, `vbmeta --flags 2`, `userdata` = `sda33`, `super` = `sda20`) | **Verified working** |
| Display & Backlight | Auto-wakes on boot into Plasma Mobile; `card1-DSI-1` 1080x2400 @ 90Hz/60Hz; `brightness=1500 actual_brightness=1500`; dynamic brightness works | **Verified working** |
| Touchscreen & Keys | `Goodix Berlin Capacitive TouchScreen` (full Plasma Mobile lockscreen + OOBE completed), `pmic_pwrkey`, `pmic_resin`, `gpio-keys`, `lisa Headset Jack` | **Verified working** |
| GPU (Adreno 642L) | `a660_sqe.fw` + `a660_gmu.bin` (`GMU v3.1.10`), `/dev/dri/card1` + `/dev/dri/renderD128`, `kwin_wayland` hardware compositing | **Verified working** |
| Audio (Speakers) | `card 0: lisa`, TFA9873 `3-0034`/`3-0035`, `PulseAudio 17.0` `alsa_output.platform-sound.HiFi__Speaker__sink` auto-starts on boot, audible sound confirmed | **Verified working** |
| Wi-Fi (`wlan0`) | WCN6750 (`ath11k`, `wpss` running); auto-connects to WPA2 network (`192.168.8.120/24`), internet `ping 1.1.1.1` (`85.3 ms`) | **Verified working** |
| Bluetooth (`hci0`) | WCN6750 controller `02:00:A0:B8:2D:D3` powers on (`Powered: yes`), discovery starts cleanly | **Verified working** |
| Modem (5G/LTE/UMTS) | `ModemManager` detects `/org/freedesktop/ModemManager1/Modem/0` (`MPSS.HI.4.3.c2`, `qrtr0` + `rmnet_ipa0`); `sim-missing` (no SIM inserted during test) | **Detected (untested with SIM)** |
| Sensors | `hexagonrpcd` + `iio-sensor-proxy`; live accelerometer, ambient light (`192 -> 65 lux`), compass (`47.8° -> 47.3°`) | **Verified working** |
| Battery & Charging | `qcom-battmgr-bat` (`Charging`, `99%`, `4.46V`), `qcom-battmgr-usb` (`online=1`, `4.94V`); harmless dmesg `0x13` payload-length warning from Xiaomi ADSP | **Verified working** |
| USB Networking & SSH | `cdc_ncm` gadget (`172.16.42.1`), SSH & SCP working | **Verified working** |
| Remoteprocs | All 4 (`modem`, `adsp`, `wpss`, `cdsp`) `running` | **Verified working** |
| Flash LED / SDHCI | `/sys/class/leds/white:flash` (`max=255`), `mmc2` (`8804000.mmc` ADMA 64-bit) registered | **Registered** |
| Video decode/encode | `/dev/video0` (`qcom-iris-decoder`), `/dev/video1` (`qcom-iris-encoder`; upstream SM7325 encoder reset caveat remains) | **Registered (known encoder caveat)** |
| Camera | `camss` ISP + `/dev/media0` + `/dev/v4l-subdev27` (`imx355 19-001a`, 8MP rear ultrawide) bound cleanly; `libcamera v0.8.0` loads `/usr/share/libcamera/ipa/softisp/imx355.yaml` and exposes `62. imx355 [libcamera]` / `63. Built-in Back Camera` in PipeWire/WirePlumber (`plasma-camera`); macro `s5k5e9` / wide / front sensors remain WIP | **Rear ultrawide IMX355 working in libcamera; other sensors WIP** |
| Haptics & Vibration | Custom mainline-style `aw8624` I2C `FF_RUMBLE` driver (`i2c4 @ 0x5a`, chip ID `0x24`, reset GPIO 35, LRA `205 Hz` CONT mode) + `73-xiaomi-lisa-haptics.rules` (`FEEDBACKD_TYPE=vibra`); `/dev/input/event4` (`aw8624_haptic`) verified with live `FF_RUMBLE` & `fbcli` | **Verified working** |
| Power & Volume Keys | `POWERDEVIL_NO_DDCUTIL=1` drop-in fixes `plasma-powerdevil` hang (was timing out probing CCI I2C buses); `PowerDevil` starts active and holds `handle-power-key` lock; `15 ms` debounce added to `key-volume-up`; PulseAudio `tsched=0` + `48000 Hz` + no-X11 drop-in fixes volume control & browser audio deadlocks | **Verified working** |

## 9. Resume after Antigravity quota exhaustion (2026-11-21, late evening)

The Antigravity `/goal` session (`3ce6b9e1-...`) stopped at its final step with
`RESOURCE_EXHAUSTED` after 1215 steps. Its working tree was clean (last commit
`833155b` at 21:05); all of its post-commit work was live on-device debugging
over SSH (`172.16.42.1`, password `147147`) and scratch files in its brain dir.
The investigation and its findings were resumed as follows.

### plasma-camera `DISPLAY=:0` hang — ROOT CAUSED AND FIXED

The session had bisected environment variables and proven that `DISPLAY=:0`
alone hangs `plasma-camera` while `WAYLAND_DISPLAY=wayland-1` alone streams
30 frames at ~25 ms/frame. The final step showed `kwin_xwl: Xwayland process
failed to start` and stale `X0`/`X1` sockets.

Direct check on the phone confirmed **`xwayland` was not installed at all**
(`command -v Xwayland` empty, `apk info -e xwayland` missing), while
`kwin_wayland` was launched with `--xwayland`. Any Qt app started with
`DISPLAY=:0` therefore blocked forever trying to reach a non-existent X server.

Fix applied:
* Live: `apk add xwayland` on the device. After the next boot `Xwayland`
  (`/usr/sbin/Xwayland`) runs and the `X0`/`X1` sockets are fresh.
* Permanent: added `xwayland` to `device/device-xiaomi-lisa/APKBUILD` `depends`.

Verified after reboot: `XDG_RUNTIME_DIR=/run/user/10000 WAYLAND_DISPLAY=wayland-1
DISPLAY=:0 timeout 5 plasma-camera` now finds the IMX355, starts the viewfinder
and processes 30 frames (`Debayer ... 30 frames in 768966 us, 25632 us/frame`),
whereas it produced no output at all before the fix.

### AW8624 haptics — verified working; earlier I2C timeouts were transient

A `modprobe aw8624` in the already-running session failed with
`error -ETIMEDOUT: Failed to read AW8624 chip ID` because `990000.i2c` had
accumulated a stuck bus (periodic `geni_i2c_gpi_xfer: I2C timeout` plus
`gpi 900000.dma-controller: cmd: CH STOP completion timeout` in dmesg).
Investigation compared the mainline `qup-i2c4-data-clk-state` (gpio16/gpio17,
function `qup04`, no drive-strength/bias) with stock DTBO fragment 85
(`drive-strength = <6>; bias-pull-up;`) and a DTB pinconf fix was drafted.

However, after a clean reboot with the **unmodified DTB** the module loads from
`modules-load.d/xiaomi-lisa.conf`, probes successfully (`id=0x24`), creates
`/dev/input/event4` (`aw8624_haptic`), and `fbcli -E button-pressed` triggers
feedback with **zero I2C timeouts in dmesg**. The bus hang was an artifact of the
long debugging session (repeated forced `/dev/i2c-4` register reads), so the
speculative DTS pinconf/clock change was reverted rather than shipped untested.

### Keyboard sound — upstream incompatibility (documented, not fixed)

`plasma-keyboard` 6.8.3's `Keyboard.qml` creates a `MultiSoundEffect` QML type
from `QtQuick.VirtualKeyboard`, but installed `qt6-qtvirtualkeyboard` 6.12.0
does not export that type anywhere (absent from `qmldir`, `*.qmltypes` and all
plugin binaries). This is upstream API drift, not a device issue; `paplay` works
(`PAPLAY_OK`) and audio volume is at 65%, so sound output itself is fine.
Proper fix requires patching/rebuilding `plasma-keyboard` against Qt 6.12.

## 10. Camera work (2026-11-21, continued session)

### IMX355 image upside-down — FIX IN TREE (needs flashing)

`cam -p` on the device reports `Property: Rotation = 270` for the IMX355,
which comes directly from `rotation = <270>` in
`kernel/dts/sm7325-xiaomi-lisa.dts`. That is 180 degrees off from the correct
mounting orientation, so the property has been changed to `rotation = <90>`.
A patched `boot.img` with the corrected DTB is prepared at
`/tmp/opencode/boot-cam-rotation.img` (the DTB is appended to the kernel in this
header-v0 boot image; only the DTB blob and `kernel_size` were updated).

Flashing still requires fastboot, which cannot currently be reached from
pmOS: the `nvmem-reboot-mode` driver is not built into this kernel, so
`reboot bootloader` just performs a normal reboot. An out-of-tree build of that
module was made and loads, but its probe defers (`-EPROBE_DEFER`, likely the
nvmem cell provider), so it cannot be used to enter fastboot either. Use the
physical key combination (Volume Down + Power) to reach fastboot, then:

```
fastboot flash boot_a /tmp/opencode/boot-cam-rotation.img
fastboot flash boot_b /tmp/opencode/boot-cam-rotation.img
fastboot reboot
```

### Why only one camera is exposed

The stock HyperOS DTBO declares **four** rear/front sensors:

| Stock node | Position | CSI PHY | CCI bus | Mainline status |
| :--- | :--- | :--- | :--- | :--- |
| cam-sensor0 | main wide 64MP | csiphy1 | cci1 | no driver (S5KGW3) |
| cam-sensor1 | front 20MP | csiphy0 | cci0 | no driver (S5K3T2) |
| cam-sensor2 | macro 5MP | csiphy2 | cci0 | no driver (S5K5E9) |
| cam-sensor3 | ultrawide 8MP (IMX355) | csiphy3 | cci1 | **working** |

The mainline DTS only binds the IMX355. The other sensors are powered (all
PM8008 LDOs are `regulator-always-on`) but their MCLKs are never enabled and
their reset GPIOs stay asserted, so they do not respond on the CCI buses.
`i2cdetect` on CCI buses 16-19 only sees the IMX355 at 0x1a plus EEPROMs/VCM;
releasing reset GPIOs from userspace is not enough without MCLK enable.

### Path to bring up the remaining sensors

1. **Front 20MP S5K3T2** — an upstream V4L2 driver exists (Armandas Kvietkus,
   v4 patch, 2026-10-11), already downloaded to the previous session's scratch
   dir (`s5k3t2.c` + `samsung,s5k3t2.yaml`). This is the easiest next sensor:
   add the driver to `scripts/patch-kernel-apkbuild.py`, add a `camera@<addr>`
   node on `&cci0` with vdda/vddd/vddio supplies, `CAM_CC_MCLK0_CLK`, reset
   GPIO 20, a `csiphy0_ep` endpoint in `&camss`, and a soft-IPA tuning file.
   The remaining unknown is the exact I2C address on this board.
2. **Main 64MP S5KGW3 and macro 5MP S5K5E9** — **no mainline V4L2 drivers
   exist**. The M52 mainline project (same GW3 sensor) keeps it in a
   proprietary `camx` patch, not a mainline sensor driver. Bringing these up
   requires writing/porting new kernel drivers from vendor sources plus
   libcamera tuning — a significantly larger effort.

## 11. Microphone capture enablement (2026-11-21, in progress)

Research (report: `/tmp/opencode/reports/microphone.md`) identified the audio
hardware and three software gaps. Live evidence: `arecord -D hw:0,0` fails with
`Invalid argument` and `ASoC: no backend DAIs enabled for MultiMedia1`;
PulseAudio sees 0 physical sources.

### Hardware
* **WCD9385** split codec on SoundWire (RX slave `swr0@0,4`, TX slave `swr1@0,3`),
  reset GPIO 83, supplies `vreg_l18b_1p8` / `vreg_l17b_1p8` / `vreg_bob`.
* 4 active analog microphones: AMIC1 main/bottom, AMIC2 headset (CTIA), AMIC4
  back, AMIC5 top (AMIC3 unused). Route: AMIC -> WCD ADC1..4 -> SoundWire TX ->
  LPASS TX macro -> Q6AFE `TX_CODEC_DMA_TX_3` (port 120) -> Q6ASM MultiMedia3.

### Changes made (in tree, pending flash/verification)
* `kernel/dts/sm7325-xiaomi-lisa.dts`: added WCD9385 codec node + GPIO83 reset
  pinconf, enabled `swr0`/`swr1` with WCD RX/TX slaves, enabled
  `lpass_tx_macro`, added q6afedai port 120 and q6asmdai ports 1/2, and added
  `audio-routing`, MM2/MM3 frontends and a `WCD Capture` DAI link to `&sound`.
  DTS compiles cleanly. Patched boot image staged at `output/images/boot-mic.img`.
* `audio/alsa-ucm-conf-lisa/HiFi.conf`: added capture verb csets and a
  `SectionDevice."Mic"` with `CapturePCM "hw:${CardId},2"`; pkgrel bumped to 4.
  The new profile was also copied live onto the device for testing.

Verification after flash: `lsmod | grep wcd938`, `arecord -D hw:0,2 ...`,
`pactl list sources` (expect `alsa_input.platform-sound.HiFi__Mic__source`),
and a speaker playback regression check.

## 12. Fingerprint sensor — research verdict (2026-11-21)

Report: `/tmp/opencode/reports/fingerprint.md`.

* The sensor is a **side-mounted capacitive FPC1020-family** part (Xiaomi "Arc
  side fingerprint sensor"), **not** an under-display optical Goodix part.
  Stock DTBO fragment 80: `compatible = "fpc,fpc1020"`, SPI QUP14 / `&spi14`
  (GPIO 56/57/58 + CS 59), reset GPIO 43, IRQ GPIO 34, supply L3C = 3.3 V.
  The repo DTS already reserves GPIO 56-59 in `gpio-reserved-ranges`.
* **No mainline kernel driver exists**; the vendor `fpc1020_tee.c` only handles
  power/reset/IRQ, while matching happens in a proprietary signed QSEE trustlet.
  `fprintd`/`libfprint` are installed but see no device, and the shipped
  `kde-fingerprint` PAM stack includes a nonexistent `fingerprint-auth` service.
* **Verdict: not a quick win.** Two long paths: (A) REE raw-SPI imaging plus a
  new libfprint driver (uncertain — the SPI block may be secure-world owned), or
  (B) TEE/QSEE trustlet extraction and reverse engineering (weeks of work,
  legally uncertain for redistribution). Not attempted in this session.
