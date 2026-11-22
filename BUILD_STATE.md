# Build state / resume notes

GitHub repo: https://github.com/Abhi-Flex1/lisa-pmos

## What is saved on disk (survives shutdown)

* `.pmos_cache/` — pmbootstrap workdir (chroots, APKs). Gitignored, on local disk.
* `pmaports/device/community/linux-postmarketos-qcom-sc7280/.kernel-build-cache/`
  — the **full compiled kernel tree with all 4356 object files**, moved here on
  2026-11-20 so a shutdown will not lose it. `prepare()` restores it automatically.
* `output/packages/*.apk` — copies of all built APKs (kept outside `.pmos_cache`).
* `output/images/` — exported images from the earlier successful build
  (symlinks point into `.pmos_cache`; re-export after reboot with
  `pmbootstrap export output/images`).
* `reference/` — stock ROM notes (large blobs are gitignored, re-extract from
  `/home/abhi/Desktop/lisa_stock/lisa_stock.tgz` if needed).

## Resume after reboot

1. `cd /home/abhi/Desktop/lisa-PMOS`
2. Check `build2.log` — if the build finished, images are in `output/images/`.
3. If it was interrupted mid-build:
   * `pmbootstrap shutdown`
   * `sudo chown -R 12345:12345 .pmos_cache/packages`
   * `./scripts/build.sh plasma-mobile` — the kernel compile may restart, but
     all APKs in `output/packages` are preserved and will be re-staged.
4. To avoid a full kernel rebuild, the compiled kernel tree is at:
   `.pmos_cache/chroot_native/home/pmos/build/src/linux-7.2.9-sc7280`
   (do NOT delete `.pmos_cache`).
5. Once images exist: put phone in fastboot and run `./scripts/flash.sh`.

## Camera tuning / names are now tracked (2026-11-22)

* `camera/libcamera-lisa/` is the tracked fork of the libcamera aport. It carries
  the calibrated `s5kgw3`, `s5k5e9`, `s5k3t2` and `imx355` SoftISP YAMLs plus the
  quad-Bayer/output-size patches. `scripts/build.sh` stages it into
  `pmaports/temp/libcamera` on every build and removes `pmaports/main/libcamera`
  so a fresh checkout cannot silently fall back to the stock tuning.
* `device/device-xiaomi-lisa/50-lisa-cameras.conf` gives the PipeWire camera
  **nodes** friendly names (Main / Ultrawide / Macro / Front). The `wpctl status`
  *Devices* section still shows sensor model names (`s5kgw3`, ...): those come
  from the `spa-libcamera` plugin and override WirePlumber's `device.description`
  / `device.name` updates after the rules run. Apps (PipeWire Sources) see the
  friendly names.
* DTB-only fix without a kernel rebuild: `scripts/patch-boot-dtb-rotation.py`
  changes the macro `rotation` cell in the DTB appended to an existing
  `boot.img` (270 -> 90) in place. The r6 boot image was patched this way and
  written to `boot_a`/`boot_b` directly over SSH with `dd` (no fastboot, no
  rootfs rewrite). DTB change takes effect on the next normal reboot.

## Do not delete

`.pmos_cache`, `output/packages`, `/home/abhi/Desktop/lisa_stock/lisa_stock.tgz`
