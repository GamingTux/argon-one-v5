# Argon ONE V5 OLED

[![Legacy Edition: v1.0.0 Stable](https://img.shields.io/badge/Legacy%20Edition-v1.0.0%20Stable-brightgreen?style=flat-square)](#-1-argon-1v5-oled-legacy-stabil--empfohlen)
[![Platform: Raspberry Pi 5](https://img.shields.io/badge/Platform-Raspberry%20Pi%205%20(arm64)-blue?style=flat-square)](https://www.raspberrypi.com/)
[![Display: SSD1306 128x64 I2C](https://img.shields.io/badge/Display-SSD1306%20128%C3%9764%20I2C-00e5ff?style=flat-square)](https://www.argon40.com/)
[![Next-Gen Suite: v1.1.0-alpha](https://img.shields.io/badge/Next--Gen%20Suite-v1.1.0--alpha%20WIP-orange?style=flat-square)](#-2-argon-v5-oled-experimentelle-next-gen-suite--wip)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)

Nativer, extrem ressourcenschonender **C-Treiber & Display-Daemon** für das integrierte **128×64 I2C OLED-Display** und den **Hardware-Power-Button** des **Argon ONE V5 Gehäuses** am Raspberry Pi 5.

---

## 🚀 1. `argon-1v5-oled-legacy` (Stabil / Empfohlen für den Alltag)

Der originale Python-Dienst von Argon40 (`argononed.py` + `argononeoled.py`) benötigt dauerhaft Python-Hintergrundprozesse, schaltet das Display bei jedem Seitenwechsel kurz aus und ein (Flackern) und aktualisiert die Werte auf der aktuellen Seite nur einmal pro Minute.

**`argon-1v5-oled-legacy`** bildet **alle originalen Argon ONE V5 Kacheln und Funktionen 1:1 in reinem C (~70 KB)** nach – erweitert um eine schnelle, flackerfreie **1-Sekunden-Echtzeit-Aktualisierung**:

* **100 % Original-Kacheln & Grafiken:** Alle originalen Argon ONE V5 Screens (`clock`, `cpu`, `storage`, `raid`, `ram`, `temp`, `ip`, `logo1v5`) und Schriften (`6×8`, `8×16`) sind direkt in die C-Binary einkompiliert.
* **Flackerfreies 1-Sekunden-Live-Update:** CPU-Last pro Kern, Temperatur, RAM und Uhrzeit aktualisieren sich jede Sekunde live (`refreshinterval=1`), ohne dass das Display beim Seitenwechsel blinkt.
* **Nahezu 0 % CPU-Last & 0 % Python-Ballast:** Keine `python3-luma.oled`- oder `python3-smbus`-Abhängigkeiten nötig.
* **Hardware-Power-Button (`GPIO4`) integriert:**
  * **Kurzer Druck:** Nächste OLED-Kachel bzw. Aufwecken aus dem Bildschirmschoner
  * **Doppelklick:** Sauberer System-Reboot (`systemctl reboot`)
  * **3 Sekunden Halten:** Sauberer Shutdown (`systemctl poweroff`)
* **Sofort einsatzbereit nach einer frischen OS-Neuinstallation (100 % Standalone):**
  * Benötigt **weder** das originale `argon1v5.sh`-Skript **noch** manuelles `raspi-config`.
  * Aktiviert bei der Paketinstallation automatisch `dtparam=i2c_arm=on` und lädt `i2c-dev` sofort zur Laufzeit (**das Display springt direkt bei der Installation an – ohne Neustart**).
  * Richtet automatisch die vorderen USB-2.0-Anschlüsse (`dtoverlay=dwc2,dr_mode=host`), `usb_max_current_enable=1` sowie die Shutdown-Erkennung (`dtparam=uart0=on` + Systemd-Shutdown-Hook) ein.

### 📦 Schnellinstallation (Legacy Edition)
```bash
wget https://github.com/GamingTux/argon-v5-oled/releases/download/v1.0.0-legacy/argon-1v5-oled-legacy_1.0.0_arm64.deb
sudo apt install ./argon-1v5-oled-legacy_1.0.0_arm64.deb
```

### ⚙️ Konfiguration (Legacy Edition)
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

## 🧪 2. `argon-v5-oled` (Experimentelle Next-Gen Suite / WIP)

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
wget https://github.com/GamingTux/argon-v5-oled/releases/download/v1.1.0-alpha/argon-v5-oled_1.1.0-alpha_arm64.deb
sudo apt install ./argon-v5-oled_1.1.0-alpha_arm64.deb
```

> [!NOTE]
> Beide Pakete erkennen sich gegenseitig (sowie den originalen `argononed.service`) und deaktivieren bei der Installation automatisch den jeweils anderen Dienst, sodass problemlos zwischen **Legacy Edition** und **Next-Gen Suite** gewechselt werden kann.

---

## 📂 Projektstruktur & Selbst-Kompilieren

```text
argon-v5-oled/
├── legacy/               (Stabiler Legacy C-Daemon, eingebettete Original-Bitmaps, argonone-oledconfig)
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

make              # Baut alle C-Binaries (argon-1v5-oled-legacy, argon-v5-oled, argon-oled)
make deb-legacy   # Baut dist/argon-1v5-oled-legacy_1.0.0_arm64.deb
make deb          # Baut dist/argon-v5-oled_1.1.0-alpha_arm64.deb
```

---

## 📄 Lizenz

Dieses Projekt ist unter der [MIT Lizenz](LICENSE) lizenziert.  
Die Software wird „wie besehen“ (AS IS) ohne jegliche Gewährleistung zur Verfügung gestellt.
