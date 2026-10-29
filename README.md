# Argon ONE V5 — Native C Drivers, Case Essentials & OLED Display Suite for Raspberry Pi 5

[![OLED Legacy: v1.2.0 Stable](https://img.shields.io/badge/OLED%20Legacy-v1.2.0%20Stable-brightgreen?style=flat-square)](#-1-argon-one-v5-oled-legacy-stable--recommended-for-oled-users)
[![Case Essential: v1.0.0 Stable](https://img.shields.io/badge/Case%20Essential-v1.0.0%20Stable-00b4d8?style=flat-square)](#-2-argon-one-v5-essential-hardware--performance-setup-for-all-cases)
[![OLED Experimental: v1.1.0 Alpha](https://img.shields.io/badge/OLED%20Experimental-v1.1.0%20Alpha-orange?style=flat-square)](#-3-argon-one-v5-oled-experimental-next-gen-suite--visual-card-studio)
[![Platform: Raspberry Pi 5 (arm64)](https://img.shields.io/badge/Platform-Raspberry%20Pi%205%20(arm64)-c51a4a?style=flat-square)](https://www.raspberrypi.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)

**Argon ONE V5** is a modular, ultra-lightweight, pure-C driver and hardware configuration suite for the **Argon ONE V5 Raspberry Pi 5 Aluminum Case** (supporting both standard **non-OLED** cases and **OLED** configurations, including **M.2 NVMe PCIe Gen 3** expansion).

It replaces Argon40's heavy Python background scripts (`argon1v5.sh` / `argononed.py` / `argononeoled.py`) with zero-overhead native Debian packages (`.deb`) that work **out of the box on a fresh Raspberry Pi OS installation**.

---

## 📦 Package Overview — Choose Your Setup

Because the **Argon ONE V5** case can be operated **with or without** the optional 128×64 I2C OLED display module, the project is cleanly split into **three independent Debian packages**:

| Package | File Name | Target Audience & Purpose |
| :--- | :--- | :--- |
| **1. [Argon ONE V5 OLED Legacy](#-1-argon-one-v5-oled-legacy-stable--recommended-for-oled-users)** | [`argon-one-v5-oled-legacy_1.2.0_arm64.deb`](https://github.com/GamingTux/argon-one-v5/releases/tag/v1.2.0-legacy) | **Stable / Production OLED Driver:** Pure-C 1:1 recreation of all original Argon ONE V5 OLED screens, fonts, and GPIO4 button actions—upgraded with **30 Hz SSD1306 partial updates**, **smooth vertical digit roll transitions**, **30 FPS bar animation**, **live `HH:MM:SS` clock**, calibrated thermometer bar, and **0% Python dependencies**. Contains strictly OLED-related configuration (`i2c-dev`). |
| **2. [Argon ONE V5 Essential](#-2-argon-one-v5-essential-hardware--performance-setup-for-all-cases)** | [`argon-one-v5-essential_1.0.0_arm64.deb`](https://github.com/GamingTux/argon-one-v5/releases/tag/v1.2.0-legacy) | **Universal Case & Performance Setup (With or Without OLED):** Configures Front USB 2.0 ports (`dwc2`), UART0 shutdown power-cut, M.2 NVMe PCIe Gen 3, safe 2.8 GHz CPU boost, a 4-stage kernel PWM fan curve, and safe I/O & CPU governor optimizations. |
| **3. [Argon ONE V5 OLED Experimental](#-3-argon-one-v5-oled-experimental-next-gen-suite--visual-card-studio)** | [`argon-one-v5-oled-experimental_1.1.0_arm64.deb`](https://github.com/GamingTux/argon-one-v5/releases/tag/v1.1.0-alpha) | **Alpha / Work in Progress:** Next-generation OLED dashboard with a Desktop Control Center, WYSIWYG Visual Card Studio (`argon-card-builder`), hardware sensor scanner, and handcrafted **1-bit Terminus Bold** pixel typography. |

---

## 🚀 1. Argon ONE V5 OLED Legacy (Stable — Recommended for OLED Users)

The official Argon40 Python daemon (`argononed.py` + `argononeoled.py`) relies on `python3-luma.oled`, blanks the screen on every page switch (causing visible flicker), re-opens font files on every text draw, and only refreshes values once every 60 seconds.

**`argon-one-v5-oled-legacy` (`v1.2.0`)** replaces the entire Python stack with a **single ~70 KB native C binary**:

* **100% Original Look & Feel + Fixes:** All original Argon ONE V5 background bitmaps (`clock`, `cpu`, `storage`, `raid`, `ram`, `temp`, `ip`, `logo1v5`) and bitmap fonts (`6×8`, `8×16`) are compiled directly into the C executable—now featuring a **live `HH:MM:SS` seconds clock** and a **tick-calibrated thermometer mercury bar** on the `temp` card (`20 °C – 85 °C`).
* **30 Hz Vertical Digit Roll Transitions + Smooth Bar Animation & SSD1306 Partial Updates:** Updates numeric text once per second (`1 Hz`) with a **smooth 30 FPS vertical digit roll animation** (only changed digits roll upward), animates CPU & thermometer bars at **30 FPS (`33 ms`)**, and uses SSD1306 Hardware Window Addressing (`0x21`/`0x22`) to transmit **only the exact pixels that changed**.
* **Near-Zero CPU & RAM Footprint:** Uses `< 0.05%` CPU and `~1.5 MB` RAM with zero Python runtime dependencies.
* **Instant GPIO4 Button Page Switching (`libgpiod` v2):**
  * **Any Press in `OLEDSWITCH` Mode:** Reliably switches to the next OLED page or wakes the display from screensaver (with hardware pull-up bias `GPIOD_LINE_BIAS_PULL_UP` and 15ms hardware debounce for RP1 GPIO4).
* **Pure OLED Setup (Instant Start Without Reboot):**
  * Automatically enables `dtparam=i2c_arm=on` and loads the `i2c-dev` kernel module immediately at runtime—the OLED display lights up **instantly upon package installation without requiring a reboot or `raspi-config`**.
  * Fixes the upstream config filename bug (`/etc/argononeoled.conf` vs. `/etc/argoneonoled.conf`) and supports **live hot-reload** whenever the configuration is modified.

### Quick Install — `Argon ONE V5 OLED Legacy`
```bash
wget https://github.com/GamingTux/argon-one-v5/releases/download/v1.2.0-legacy/argon-one-v5-oled-legacy_1.2.0_arm64.deb
sudo apt install ./argon-one-v5-oled-legacy_1.2.0_arm64.deb
```

### Interactive OLED Configuration
```bash
sudo argonone-oledconfig
```
Or edit `/etc/argononeoled.conf` directly (changes are applied automatically via live hot-reload):
```ini
enabled=Y
switchduration=10
screensaver=120
refreshinterval=1
screenlist="clock cpu storage ram temp ip"
```

---

## ⚡ 2. Argon ONE V5 Essential (Hardware & Performance Setup for All Cases)

**`argon-one-v5-essential`** configures the **Argon ONE V5 case hardware and safe Raspberry Pi 5 performance optimizations**. It works on **every Argon ONE V5 case**—whether you have the standard non-OLED version or the OLED version.

### What It Configures Automatically:
1. **Argon ONE V5 Case Hardware (`/boot/firmware/config.txt`):** *(Existing custom values are never overwritten)*
   * **Front USB 2.0 Ports & Full USB Current:** Enables `dtoverlay=dwc2,dr_mode=host` and `usb_max_current_enable=1`.
   * **Clean Shutdown Power-Cut Detection:** Enables `dtparam=uart0=on` so the Argon ONE V5 case microcontroller detects Raspberry Pi 5 power-off state.
   * **M.2 NVMe Slot & PCIe Gen 3 Speed:** Enables `dtparam=nvme` and `dtparam=pciex1_gen=3` (doubling NVMe SSD throughput from ~450 MB/s to **~850–900 MB/s**).
   * **Safe 2.8 GHz CPU Boost:** Sets `arm_freq=2800` (if no custom `arm_freq` is present) while leaving `v3d_freq` at standard firmware defaults and requiring zero manual over-voltage.
   * **Responsive 4-Stage Kernel PWM Fan Curve:** Configures the Raspberry Pi 5 native PWM fan controller directly via device-tree parameters (`40°C -> 100`, `50°C -> 150`, `60°C -> 200`, `70°C -> 250` with `5°C` hysteresis)—requiring **0% background CPU overhead**.
2. **Safe System & Storage Optimizations:**
   * **Fast CPU Governor Ramp-Up (`ondemand`):** Tunes `up_threshold=50`, `sampling_down_factor=2`, and `io_is_busy=1` for snappy desktop and workload responsiveness while preserving cool 1.5 GHz idle clocks.
   * **Memory & VFS Cache Tuning (`/etc/sysctl.d/90-argon-v5-perf.conf`):** Sets `vm.swappiness=10` and `vm.vfs_cache_pressure=50` to reduce unnecessary SSD/SD wear.
   * **NVMe Direct Multiqueue & PCIe Latency (`/etc/udev/rules.d/60-argon-v5-perf.rules`):** Sets `scheduler=none`, `read_ahead_kb=512`, `rq_affinity=2`, and prevents PCIe Gen 3 ASPM sleep latency spikes.

### Quick Install & Status Check — `Argon ONE V5 Essential`
```bash
wget https://github.com/GamingTux/argon-one-v5/releases/download/v1.2.0-legacy/argon-one-v5-essential_1.0.0_arm64.deb
sudo apt install ./argon-one-v5-essential_1.0.0_arm64.deb

# Inspect active case hardware and performance settings:
argon-v5-essential --status
```

---

## 🧪 3. Argon ONE V5 OLED Experimental (Next-Gen Suite & Visual Card Studio)

> [!WARNING]
> **Experimental Alpha Status:** **`argon-one-v5-oled-experimental`** is under active development (Work in Progress). For a rock-solid daily driver with the classic Argon screens, install **[`argon-one-v5-oled-legacy`](#-1-argon-one-v5-oled-legacy-stable--recommended-for-oled-users)** instead. Both OLED packages automatically conflict/replace each other cleanly when switching.

### Key Features (`v1.1.0`)
* **Handcrafted 1-Bit Terminus Bold Pixel Typography:**
  Crisp, zero-blur 1-bit bitmap rendering in three sizes (`6×12`, `8×16`, `11×22`) with native UTF-8 support for German umlauts and symbols (`Ä Ö Ü ä ö ü ß ° ↑ ↓`) across both the C daemon and the Visual Card Studio.
* **Visual Card Builder & Screen Studio (`argon-card-builder`):**
  4× zoomed 128×64 WYSIWYG canvas, built-in 1-bit SVG/PNG/XBM icon converter, portable `.argoncard` package bundler, and one-click live preview on the physical OLED screen.
* **Desktop Control Center (`argon-oled-desktop`):**
  Real-time pixel-accurate OLED mirror via shared memory (`/dev/shm/argon_oled_fb.bin`) and interactive daemon configuration.
* **Hardware & Sensor Discovery Engine (`argon-hw-discover` / `argon-oled sensors`):**
  Automatically scans Raspberry Pi 5 thermal zones, RP1 ADC, PMIC rails, NVMe SMART data, RAID arrays, network interfaces, GPIO lines, and I2C buses.

<p align="center">
  <img src="screenshots/control_center.png" alt="Argon ONE V5 OLED Desktop Control Center" width="550">
</p>

<p align="center">
  <img src="screenshots/card_builder.png" alt="Argon ONE V5 OLED Visual Card Studio" width="750">
</p>

### Quick Install — `Argon ONE V5 OLED Experimental`
```bash
wget https://github.com/GamingTux/argon-one-v5/releases/download/v1.1.0-alpha/argon-one-v5-oled-experimental_1.1.0_arm64.deb
sudo apt install ./argon-one-v5-oled-experimental_1.1.0_arm64.deb
```

### CLI Commands (`argon-oled`)
```bash
argon-oled sensors        # Discover all Raspberry Pi 5 hardware sensors
argon-oled sensors --json # Machine-readable JSON sensor report
argon-oled next           # Switch to next card
argon-oled prev           # Switch to previous card
argon-oled screen 1       # Jump directly to card index
argon-oled toggle         # Toggle OLED display power
argon-oled dim            # Dim OLED brightness
argon-oled status         # Show daemon status
```

---

## 🛠️ Repository Structure & Building from Source

```text
argon-one-v5/
├── legacy/               # Argon ONE V5 OLED Legacy (Pure-C daemon, embedded .bin assets, argonone-oledconfig)
├── essentials/           # Argon ONE V5 Essential (Case hardware setup, NVMe Gen3, fan curve & sysctl/udev tuning)
├── src/                  # Argon ONE V5 OLED Experimental (Next-Gen C daemon & CLI with Terminus Bold 1-bit fonts)
├── oled_engine.py        # 1-bit pixel rendering engine for the Visual Card Studio
├── argon_card_builder.py # Python/Tkinter: WYSIWYG Visual Card Studio & 1-bit XBM/SVG Converter
├── argon-oled-desktop    # Python/Tkinter: Desktop Control Center with live shared-memory OLED mirror
├── argon_hw_discovery.py # Hardware & Sensor Discovery Engine for Raspberry Pi 5
└── dist/                 # Pre-built arm64 Debian packages (.deb) & SHA256SUMS
```

### Build All Packages from Source (Debian / Raspberry Pi OS Bookworm & Trixie):
```bash
sudo apt update
sudo apt install -y build-essential libgpiod-dev libgpiod3 i2c-tools python3 python3-tk python3-pil

make packages       # Builds all 3 .deb packages in dist/
# Or individually:
make deb-legacy       # -> dist/argon-one-v5-oled-legacy_1.2.0_arm64.deb
make deb-essential    # -> dist/argon-one-v5-essential_1.0.0_arm64.deb
make deb-experimental # -> dist/argon-one-v5-oled-experimental_1.1.0_arm64.deb
```

---

## 🔍 Keywords & Compatibility

* **Hardware:** Argon ONE V5 Case, Argon ONE V5 OLED Module, Argon ONE V5 M.2 NVMe PCIe Slot, Raspberry Pi 5 (4GB / 8GB / 16GB), SSD1306 128×64 I2C OLED (`0x3c`), BCM2712 / RP1 GPIO4 (`gpiochip4` / `gpiochip0`).
* **Operating Systems:** Raspberry Pi OS (64-bit Bookworm / Trixie), Debian 12/13 `arm64`, Ubuntu 24.04+ `arm64` for Raspberry Pi 5.
* **Replaces:** `argon1v5.sh`, `argononed.py`, `argononeoled.py`, `argoneonoled.py`, `argon-config`.

---

## 📄 License

Licensed under the [MIT License](LICENSE). Provided "AS IS" without warranty of any kind.
