# Stock ROM reference (Xiaomi 11 Lite 5G NE / lisa)

Source archive: `lisa_stock.tgz` (6.1 GB) on the original build machine at
`/home/abhi/Desktop/lisa_stock/lisa_stock.tgz`.

Extracted path inside archive:
`lisa_global_images_OS2.0.8.0.UKOMIXM_14.0/images/`

## Build identity

* Region/variant: `lisa_global`
* HyperOS: `OS2.0.8.0.UKOMIXM` (Android 14 base)
* Fingerprint reported by device after flash:
  `Xiaomi/lisa_global/lisa:14/UKQ1.240624.001/OS2.0.8.0.UKOMIXM:user/release-keys`
* Kernel on stock: `5.4.289-qgki-g5987d69e25da`

## Important files in the archive

`boot.img`, `boot-gki.img`, `vendor_boot.img`, `vbmeta.img`, `vbmeta_system.img`,
`dtbo.img`, `dtb.img`, `super.img`, `userdata.img`, `rawprogram0-5.xml`,
`patch0-5.xml`, `prog_firehose_ddr.elf`, `prog_firehose_lite.elf`,
`gpt_main*.bin`, `gpt_both*.bin`.

## Extracted copies in this repo

Small files only (see `reference/`): `vbmeta.img`, `dtb.img`,
`prog_firehose_lite.elf`, `stock-boot-header.txt`.

The full `boot.img` (192 MB) and `vendor_boot.img` (96 MB) are too large for
GitHub; extract them from the archive when needed.

## vbmeta info (from `avbtool info_image`)

```
Flags: 0
Algorithm: SHA256_RSA2048
Descriptors: chain partition vbmeta_system + build fingerprint props for
              boot, vendor_boot, vendor
```
