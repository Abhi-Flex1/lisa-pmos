# Xiaomi lisa fingerprint (side-mounted FPC1020 via TrustZone)

The sensor's SPI bus and GPIOs 56-59 are owned by the secure world; a REE
bit-bang driver hard-freezes early boot and is deliberately not built
(`kernel/drivers/fpc1020.c` is kept only as a reference). The working path is
the QSEE trustlet `fpclisa`, reached through `/dev/qseecom_mobile`.

## Verified on hardware (2026-11-22)

* `/lib/firmware/fpclisa.mdt` + `.b00`-.`b07` are present in the shipped rootfs.
* `qcom_qseecom_mobile` loads and creates `/dev/qseecom_mobile` (10:263).
* `QSEECOM_MOBILE_IOC_LOAD_APP("fpclisa")` succeeds with `app_id = 5`.
* `QSEECOM_MOBILE_IOC_SEND_CMD` returns `rc=0` with a 4-byte TEE response,
  `29 00 50 e8` (little-endian `0xe8500029`), for simple probe payloads —
  not `-EINVAL`. A sweep of 768 request headers (first u32 0..255, with and
  without `0x205`/`0x200` markers) always returned this same error, so the TA
  expects a larger structured request. The command protocol is still unknown.

The stock ROM's `vendor/lib64/hw/fingerprint.fpc.so` was recovered from
`super.img` and depends on `libQSEEComAPI.so` (`QSEECom_start_app`,
`QSEECom_send_cmd`, `QSEECom_send_modified_cmd`, `QSEECom_shutdown_app`); the
sensor is an FPC1540 (the vendor also ships `fpc1540.ko`). The HAL exposes
`fpc_tee_*` entry points but its request layout has not been decoded yet.

`qseecom_probe.py` is the working userspace probe. Important: Linux ioctl
numbers use `dir<<30 | size<<16 | type<<8 | nr`; swapping `nr` and `size`
makes every ioctl return `ENOTTY` even though the driver is fine.

## Remaining work

* Reverse-engineer the `fpclisa` command set (enrol/match/get-image) from the
  stock `vendor/lib64/hw/fingerprint.fpc.so` and/or QSEE logs.
* Write a `libfprint` driver or a small daemon that bridges the TA to
  `fprintd`; the stock PAM stack references a `fingerprint-auth` service that
  does not exist yet.
