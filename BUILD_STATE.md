# Build state / resume notes

GitHub repo: https://github.com/Abhi-Flex1/lisa-pmos-mainline

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

## Do not delete

`.pmos_cache`, `output/packages`, `/home/abhi/Desktop/lisa_stock/lisa_stock.tgz`
