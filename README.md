# Argon ONE V5 OLED & Case Essentials

[![Legacy OLED: v1.0.0 Stable](https://img.shields.io/badge/Legacy%20OLED-v1.0.0%20Stable-brightgreen?style=flat-square)](#-1-argon-1v5-oled-legacy-stabil--reiner-oled-treiber--tools)
[![Case Essentials: v1.0.0](https://img.shields.io/badge/Case%20Essentials-v1.0.0%20Stable-00b4d8?style=flat-square)](#-2-argon-one-v5-essentials-allgemeine-geh%C3%A4use---performance-einrichtung)
[![Platform: Raspberry Pi 5](https://img.shields.io/badge/Platform-Raspberry%20Pi%205%20(arm64)-blue?style=flat-square)](https://www.raspberrypi.com/)
[![Next-Gen Suite: v1.1.0-alpha](https://img.shields.io/badge/Next--Gen%20Suite-v1.1.0--alpha%20WIP-orange?style=flat-square)](#-3-argon-v5-oled-experimentelle-next-gen-suite--wip)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)

Modulare, extrem ressourcenschonende **C-Treiber, OLED-Tools und Gehäuse-Optimierungen** für das **Argon ONE V5 Gehäuse** am Raspberry Pi 5.  
Da das Argon ONE V5 sowohl **mit** als auch **ohne** OLED-Display-Modul betrieben werden kann, sind **OLED-Treiber** und **allgemeine Gehäuse-Konfiguration** sauber in eigene Pakete getrennt:

| Paket | Zweck |
| :--- | :--- |
| **[`argon-1v5-oled-legacy_1.0.0_arm64.deb`](#-1-argon-1v5-oled-legacy-stabil--reiner-oled-treiber--tools)** | **Reiner OLED-Treiber & OLED-Tools (Stabil):** 1:1 Nachbildung aller originalen Argon-OLED-Kacheln in C mit **1s-Live-Refresh**, Button-Steuerung und `argonone-oledconfig`. Enthält ausschließlich die für den OLED-Betrieb nötigen Einstellungen (`i2c_arm=on`, `i2c-dev`). |
| **[`argon-one-v5-essentials_1.0.0_arm64.deb`](#-2-argon-one-v5-essentials-allgemeine-gehäuse---performance-einrichtung)** | **Allgemeine Gehäuse- & Performance-Einrichtung (Mit & ohne OLED nutzbar):** Aktiviert Front-USB-2.0 (`dwc2`), `uart0`, M.2 NVMe mit PCIe Gen 3, sichere 2800-MHz-CPU-Taktrate, 4-Stufen-PWM-Lüfterkurve sowie CPU-Governor-, RAM- und NVMe-Queue-Optimierungen. |
| **[`argon-v5-oled_1.1.0-alpha_arm64.deb`](#-3-argon-v5-oled-experimentelle-next-gen-suite--wip)** | **Experimentelle Next-Gen OLED Suite (Alpha / WIP):** Neues Dashboard mit Desktop Control Center, visuellem Kachel-Designer (`argon-card-builder`) und handoptimierter 1-Bit Terminus-Bold-Schrift. |

---

## 🚀 1. `argon-1v5-oled-legacy` (Stabil — Reiner OLED-Treiber & Tools)

Der originale Python-Dienst von Argon40 (`argononed.py` + `argononeoled.py`) benötigt dauerhaft Python-Hintergrundprozesse, schaltet das Display bei jedem Seitenwechsel kurz aus und ein (Flackern) und aktualisiert die Werte auf der aktuellen Seite nur einmal pro Minute.

**`argon-1v5-oled-legacy`** konzentriert sich rein auf das **OLED-Display und die zugehörigen Werkzeuge** und bildet alle originalen Argon ONE V5 Kacheln 1:1 in reinem C (~70 KB) nach:

* **100 % Original-Kacheln & Grafiken:** Alle originalen Argon ONE V5 Screens (`clock`, `cpu`, `storage`, `raid`, `ram`, `temp`, `ip`, `logo1v5`) und Schriften (`6×8`, `8×16`) sind direkt in die C-Binary einkompiliert.
* **Flackerfreies 1-Sekunden-Live-Update:** CPU-Last pro Kern, Temperatur, RAM und Uhrzeit aktualisieren sich jede Sekunde live (`refreshinterval=1`), ohne dass das Display beim Seitenwechsel blinkt.
* **Nahezu 0 % CPU-Last & 0 % Python-Ballast:** Keine `python3-luma.oled`- oder `python3-smbus`-Abhängigkeiten nötig.
* **Hardware-Power-Button (`GPIO4`) integriert:**
  * **Kurzer Druck:** Nächste OLED-Kachel bzw. Aufwecken aus dem Bildschirmschoner
  * **Doppelklick:** Sauberer System-Reboot (`systemctl reboot`)
  * **3 Sekunden Halten:** Sauberer Shutdown (`systemctl poweroff`)
* **Reine OLED-Einrichtung ohne Nebeneffekte:**
  * Aktiviert bei der Paketinstallation lediglich den für das Display nötigen I2C-Bus (`dtparam=i2c_arm=on`) und lädt `i2c-dev` sofort zur Laufzeit (**das Display springt direkt bei der Installation an – ohne Neustart**).

### 📦 Installation (Legacy OLED Edition)
```bash
wget https://github.com/GamingTux/argon-one-v5-oled/releases/download/v1.0.0-legacy/argon-1v5-oled-legacy_1.0.0_arm64.deb
sudo apt install ./argon-1v5-oled-legacy_1.0.0_arm64.deb
```

### ⚙️ OLED-Konfiguration
Per interaktivem Menü im Terminal:
```bash
sudo argonone-oledconfig
```
Oder direkt in `/etc/argononeoled.conf` (Änderungen werden im laufenden Betrieb per **Live-Hot-Reload** sofort übernommen):
```ini
enabled=Y
switchduration=10
screensaver=120
refreshinterval=1
screenlist="clock cpu storage ram temp ip"
```

---

## ⚡ 2. `argon-one-v5-essentials` (Allgemeine Gehäuse- & Performance-Einrichtung)

Dieses separate Paket richtet das **Argon ONE V5 Gehäuse selbst** optimal ein – völlig unabhängig davon, ob ein OLED-Modul verbaut ist oder nicht:

* **Gehäuse-Hardware in `/boot/firmware/config.txt`** *(überschreibt keine bereits vorhandenen eigenen Werte)*:
  * **Front-USB-2.0-Anschlüsse & volles USB-Strombudget:** `dtoverlay=dwc2,dr_mode=host` & `usb_max_current_enable=1`
  * **Shutdown-Powercut-Erkennung:** `dtparam=uart0=on`
  * **M.2 NVMe-Slot & volle PCIe Gen 3 Geschwindigkeit:** `dtparam=nvme` & `dtparam=pciex1_gen=3` (~850–900 MB/s)
  * **Sicherer CPU-Boost auf 2800 MHz:** `arm_freq=2800` (sofern noch kein eigenes `arm_freq=` gesetzt ist; `v3d_freq` bleibt unangetastet auf Standard)
  * **Straffe, leise 4-Stufen-PWM-Lüfterkurve:** `40 °C -> 100`, `50 °C -> 150`, `60 °C -> 200`, `70 °C -> 250` (direkt über den Kernel-Device-Tree, 0 % CPU-Last)
* **Sichere System- & I/O-Performance-Optimierungen:**
  * **CPU-Governor-Tuning (`ondemand`):** Schnellere Taktreaktion bei Lastspitzen (`up_threshold=50`, `sampling_down_factor=2`, `io_is_busy=1`) bei kühlem 1,5-GHz-Leerlauf
  * **RAM- & Cache-Optimierung (`/etc/sysctl.d/90-argon-v5-perf.conf`):** `vm.swappiness=10` und `vm.vfs_cache_pressure=50`
  * **NVMe Multiqueue & PCIe-Latenz (`/etc/udev/rules.d/60-argon-v5-perf.rules`):** Direkter `none`-Scheduler, `read_ahead_kb=512`, `rq_affinity=2` und Schutz vor PCIe-ASPM-Latenzen

### 📦 Installation & Status-Check (Case Essentials)
```bash
wget https://github.com/GamingTux/argon-one-v5-oled/releases/download/v1.0.0-legacy/argon-one-v5-essentials_1.0.0_arm64.deb
sudo apt install ./argon-one-v5-essentials_1.0.0_arm64.deb

# Status aller Gehäuse- und Performance-Einstellungen prüfen:
argon-v5-essentials --status
```

---

## 🧪 3. `argon-v5-oled` (Experimentelle Next-Gen Suite / WIP)

> [!WARNING]
> **Experimenteller Alpha-Status:** Neben der stabilen Legacy-Version entsteht in diesem Projekt eine komplett neue Dashboard-Suite mit eigenem Kachel-Editor. Diese Edition befindet sich in aktiver Entwicklung (Alpha / Work in Progress).

### Highlights der Next-Gen Suite (`v1.1.0-alpha`)
* **Handoptimierte 1-Bit Terminus Bold Bitmap-Schrift:**
  Pixelperfekte 1-Bit-Darstellung ohne Anti-Aliasing-Fransen in drei Größen (`6×12`, `8×16`, `11×22`) – inklusive nativer UTF-8-Unterstützung für deutsche Umlaute und Symbole (`Ä Ö Ü ä ö ü ß ° ↑ ↓`).
* **Visueller Kachel-Designer (`argon-card-builder`):**
  128×64 WYSIWYG-Arbeitsfläche mit 4× Zoom, SVG/XBM-1-Bit-Konverter, Live-Vorschau und `.argoncard`-Paketexport zum Entwerfen eigener Kacheln.
* **Desktop Control Center (`argon-oled-desktop`):**
  Grafisches Kontrollzentrum mit Echtzeit-Spiegelung des Displays über `/dev/shm/argon_oled_fb.bin`.
* **Hardware & Sensor Discovery (`argon-hw-discover` / `argon-oled sensors`):**
  Automatische Erkennung von CPU-, RP1-, PMIC-, NVMe-, USB-, Netzwerk-, GPIO- und I2C-Sensoren auf dem Raspberry Pi 5.

<p align="center">
  <img src="screenshots/control_center.png" alt="Argon ONE V5 OLED Control Center & Monitor" width="550">
</p>

<p align="center">
  <img src="screenshots/card_builder.png" alt="Argon ONE V5 Kachel-Designer & Screen Studio" width="750">
</p>

### 📦 Installation (Experimentelle Next-Gen Suite)
```bash
wget https://github.com/GamingTux/argon-one-v5-oled/releases/download/v1.1.0-alpha/argon-v5-oled_1.1.0-alpha_arm64.deb
sudo apt install ./argon-v5-oled_1.1.0-alpha_arm64.deb
```

---

## 📂 Projektstruktur & Selbst-Kompilieren

```text
argon-v5-oled/
├── legacy/               (Stabiler Legacy OLED C-Daemon, Original-Bitmaps, argonone-oledconfig)
├── essentials/           (Separates Case- & Performance-Paket argon-one-v5-essentials)
├── src/                  (Experimenteller Next-Gen C-Daemon & CLI mit Terminus Bold Fonts)
├── oled_engine.py        (Pixel-Engine mit Terminus Bold für den visuellen Kachel-Designer)
├── argon_card_builder.py (Python/Tkinter: Visueller Kachel-Designer & Screen Studio)
├── argon-oled-desktop    (Python/Tkinter: Desktop Control Center mit Live-OLED-Spiegel)
├── argon_hw_discovery.py (Python: Hardware & Sensor Discovery Engine)
└── dist/                 (Fertig gebaute .deb-Pakete für arm64)
```

### Aus dem Quellcode bauen:
```bash
sudo apt update
sudo apt install -y build-essential libgpiod-dev libgpiod3 i2c-tools python3 python3-tk python3-pil

make                # Baut alle C-Binaries
make deb-legacy     # Baut dist/argon-1v5-oled-legacy_1.0.0_arm64.deb
make deb-essentials # Baut dist/argon-one-v5-essentials_1.0.0_arm64.deb
make deb            # Baut dist/argon-v5-oled_1.1.0-alpha_arm64.deb
```

---

## 📄 Lizenz

Dieses Projekt ist unter der [MIT Lizenz](LICENSE) lizenziert.  
Die Software wird „wie besehen“ (AS IS) ohne jegliche Gewährleistung zur Verfügung gestellt.
