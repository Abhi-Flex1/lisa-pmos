# postmarketOS for Xiaomi 11 Lite 5G NE (`xiaomi-lisa`)

This repository contains the complete porting and packaging files to build and run **postmarketOS** (mainline Linux) on the **Xiaomi 11 Lite 5G NE / 11 LE** (codename: `lisa`, model `2107119DC`).

---

## 📱 Hardware & Status Overview

The Xiaomi 11 Lite 5G NE is powered by the **Qualcomm Snapdragon 778G (SM7325)** SoC, which shares the upstream kernel tree with the **Fairphone 5 (`qcm6490`)**, **Motorola Edge 30 (`motorola-dubai`)**, and **Xiaomi 12 Lite (`xiaomi-taoyao`)**.

| Component | Hardware Specification | Status in this Port | Notes |
| :--- | :--- | :---: | :--- |
| **SoC** | Qualcomm Snapdragon 778G (SM7325) | ✅ Works | Shared `linux-postmarketos-qcom-sc7280` |
| **GPU** | Adreno 642L | ✅ Works | Mesa Turnip / Freedreno 3D acceleration |
| **Internal Storage** | Universal Flash Storage (UFS 2.2) | ✅ Works | Built-in high-speed storage |
| **External Storage** | MicroSDXC | ✅ Works | Shared hybrid SIM/SD slot |
| **Display** | 1080x2400 90Hz AMOLED (CSOT VTDR6130) | ✅ Works | Custom DRM panel driver + simple-framebuffer |
| **Touchscreen** | Goodix GT9897 (SPI Berlin-A series) | ✅ Works | `goodix_berlin` driver |
| **Power & Battery** | PM7325 + PM8350B + PM8350C + PM8008 | ✅ Works | Charging & battery level reporting |
| **Wi-Fi** | Qualcomm WCN6750 (Wi-Fi 6) | ✅ Works | Remoteproc firmware loaded |
| **Bluetooth** | Qualcomm WCN6750 (BT 5.2) | ✅ Works | Remoteproc firmware loaded |
| **Hardware Keys** | Power, Volume Up, Volume Down | ✅ Works | GPIO keys + PMIC PON |
| **Sensors & Flash** | Accel, Gyro, Compass, ALS, Camera Flash | ✅ Works | IIO and PMIC flash |
| **USB Host / OTG** | USB 2.0 Type-C (FSA4480 mux) | ✅ Works | OTG & USB Networking |
| **Audio** | Dual NXP TFA9873 (Ear @ 0x34, Spk @ 0x35) | 🔄 Configured | MI2S playback with ALSA UCM configuration |
| **Modem / Cellular** | Qualcomm Hexagon DSP / MPSS | 🔄 WIP | Modem firmware loaded; voice via `q6voice` |
| **Cameras** | Samsung GW3 (64MP) / S5K5E9 (5MP telemacro) | ⚠️ WIP | Complex V4L2 pipeline / libcamera tuning |

---

## 📁 Repository Structure

```
postmarket-lisa/
├── device/
│   └── device-xiaomi-lisa/       # postmarketOS device package
│       ├── APKBUILD              # Package recipe for pmaports
│       ├── deviceinfo            # Device capabilities, geometry, bootloader flags
│       └── modules-initfs        # Early boot modules (panel, touchscreen, USB mux)
├── firmware/
│   └── firmware-xiaomi-lisa/     # Proprietary modem, GPU, DSP firmware package
│       ├── APKBUILD              # Packaging script
│       └── *.files               # Manifests for adsp, cdsp, gpu, ipa, modem, sensors, vpu, wpss
├── kernel/
│   ├── dts/
│   │   └── sm7325-xiaomi-lisa.dts # Complete Device Tree Source
│   ├── drivers/
│   │   └── panel-csot-vtdr6130.c # DRM panel driver for CSOT AMOLED display
│   └── patches/
│       └── 0001-ASoC-qcom-sm8250-Add-Xiaomi-Lisa-soundcard-compatible.patch
├── audio/
│   └── ucm2/lisa-sndcard/        # ALSA Use Case Manager (UCM2) profiles
│       ├── lisa-sndcard.conf     # Card entry point
│       └── HiFi.conf             # Stereo routing to TFA9873 amplifiers
├── docker/
│   ├── Dockerfile                # Alpine Linux container with pmbootstrap for macOS
│   └── run-pmbootstrap.sh        # Wrapper to run pmbootstrap inside container
├── scripts/
│   ├── setup-pmaports.sh         # Link packages into local pmaports clone
│   ├── build.sh                  # One-click build script
│   ├── test-boot.sh              # Safe, non-destructive fastboot boot
│   └── flash.sh                  # Flasher script
└── README.md
```

---

## 🚀 Building the OS Image

### Option A: Building on macOS (using Docker / Colima)

Because `pmbootstrap` requires Linux kernel namespaces, chroots, and loop devices, a container runtime is used on macOS:

1. **Install and start a container runtime** (if not already installed):
   ```bash
   brew install colima docker
   colima start --cpu 4 --memory 8
   ```

2. **Run the one-click build script inside the container**:
   ```bash
   ./docker/run-pmbootstrap.sh /workspace/postmarket-lisa/scripts/build.sh phosh
   ```
   *(You can replace `phosh` with `sxmo-de-dwm`, `plasma-mobile`, or `console`)*.

3. The generated `boot.img` and rootfs image will be exported directly to the `output/` directory.

---

### Option B: Building on a Native Linux System

If you have a Linux workstation or virtual machine:

1. **Install `pmbootstrap`**:
   ```bash
   pip install --user pmbootstrap
   pmbootstrap init
   ```

2. **Run the build script**:
   ```bash
   ./scripts/build.sh phosh
   ```

---

## 🧪 Testing & Flashing to Device

### 1. Safe Non-Destructive Boot Test (Recommended First Step)
To verify that the kernel and display boot without modifying your phone's internal storage partitions:

1. Connect your device via USB.
2. Run:
   ```bash
   ./scripts/test-boot.sh
   ```
   This will automatically reboot the device to bootloader mode and issue `fastboot boot output/boot.img`.

3. **Verify over USB Networking**:
   When booted, the phone will present a USB Ethernet gadget:
   ```bash
   ssh user@172.16.42.1
   ```
   (Default password set during `pmbootstrap init`).

---

### 2. Flashing Permanently
When ready to flash:
```bash
./scripts/flash.sh output/boot.img output/*rootfs*.img
```

---

## 🛠️ Upstreaming to Official postmarketOS

To merge `xiaomi-lisa` into the official `postmarketOS/pmaports` repository:

1. **Kernel Submission**:
   - Submit a pull request containing `kernel/dts/sm7325-xiaomi-lisa.dts`, `kernel/drivers/panel-csot-vtdr6130.c`, and the soundcard patch to [sc7280-mainline/linux](https://github.com/sc7280-mainline/linux) (branch `sc7280-7.2.y`).

2. **Audio UCM Submission**:
   - Submit `audio/ucm2/lisa-sndcard/` to [sc7280-mainline/alsa-ucm-conf](https://github.com/sc7280-mainline/alsa-ucm-conf).

3. **pmaports Merge Request**:
   - Fork [postmarketOS/pmaports](https://gitlab.postmarketos.org/postmarketOS/pmaports).
   - Copy `device/device-xiaomi-lisa` and `firmware/firmware-xiaomi-lisa` into `device/testing/`.
   - Open a Merge Request on GitLab.

---

## 📜 Credits & Acknowledgments
- **Jens Reidel (`Aelin`)**: Initial device tree, touchscreen, and panel bringup.
- **Stanislav Zaikin**: Xiaomi 12 Lite (`taoyao`) SM7325 reference and audio bringup.
- **Luca Weiss (`z3ntu`)**: Fairphone 5 mainlining and `sc7280-mainline` kernel maintenance.
- **postmarketOS Community**: Packaging tooling and infrastructure.
