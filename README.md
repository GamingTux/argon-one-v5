# Argon ONE V5 OLED Display Suite & Legacy C-Daemon

[![Release: Legacy v1.0.0](https://img.shields.io/badge/Legacy%20Edition-v1.0.0%20Stable-brightgreen?style=flat-square)](#-1-argon-1v5-oled-legacy-stabil--produktiv)
[![Status: Next-Gen Alpha](https://img.shields.io/badge/Next--Gen%20Suite-v1.1.0--alpha%20WIP-orange?style=flat-square)](#-2-argon-v5-oled-experimentelle-next-gen-suite--wip)
[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%205%20(arm64)-blue?style=flat-square)](https://www.raspberrypi.com/)
[![Display](https://img.shields.io/badge/Display-SSD1306%20128%C3%9764%20I2C-00e5ff?style=flat-square)](https://www.argon40.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)

Dieses Repository bietet **zwei fertige `.deb`-Pakete** für das integrierte **128×64 I2C SSD1306 OLED-Display** und den **Hardware-Power-Button** des **Argon ONE V5 Gehäuses** am Raspberry Pi 5:

| Edition | Paket | Zielgruppe & Status |
| :--- | :--- | :--- |
| **1. Legacy Edition (Empfohlen für den Alltag)** | [`argon-1v5-oled-legacy_1.0.0_arm64.deb`](https://github.com/GamingTux/argon-v5-oled/releases) | **Stabil / Produktiv:** 1:1 Nachbildung aller originalen Argon ONE V5 Kacheln & Funktionen in reinem C – mit **1-Sekunden-Echtzeit-Refresh**, flackerfreiem Seitenwechsel und **0 % Python-Ballast**. Läuft sofort nach einer frischen OS-Installation ohne das originale Argon-Skript. |
| **2. Next-Gen Suite (Experimentell)** | [`argon-v5-oled_1.1.0-alpha_arm64.deb`](https://github.com/GamingTux/argon-v5-oled/releases) | **Alpha / Work in Progress:** Neue Dashboard-Generation mit Desktop Control Center, visuellem Kachel-Editor (`argon-card-builder`), Hardware-Sensor-Scanner und handoptimierter **1-Bit Terminus Bold** Pixel-Schrift (`ÄÖÜß°`). |

> [!NOTE]
> Beide Pakete erkennen sich gegenseitig (sowie den originalen `argononed.service`) und deaktivieren bei der Installation automatisch den jeweils anderen Dienst, sodass immer nur ein Daemon auf `0x3c` und `GPIO4` zugreift.

---

## 🚀 1. `argon-1v5-oled-legacy` (Stabil / Produktiv)

Der originale Python-Dienst von Argon40 (`argononed.py` + `argononeoled.py`) verbraucht unnötig CPU-Zeit, schaltet das Display bei jedem Seitenwechsel kurz aus und ein (Flackern) und aktualisiert die Werte auf der aktuellen Seite nur einmal pro Minute.

**`argon-1v5-oled-legacy`** ersetzt den gesamten Python-Unterbau durch einen **einzigen nativen C-Daemon (~70 KB)**:

* **100 % Original-Optik:** Alle originalen Argon ONE V5 Hintergründe (`clock`, `cpu`, `storage`, `raid`, `ram`, `temp`, `ip`, `logo1v5`) und Schriften (`6×8`, `8×16`) sind direkt in die C-Binary einkompiliert.
* **Flackerfreies 1-Sekunden-Live-Update:** CPU-Last, Temperatur, RAM und Uhrzeit aktualisieren sich jede Sekunde live (`refreshinterval=1`), und I2C-Daten werden nur übertragen, wenn sich tatsächlich Pixel geändert haben.
* **Hardware-Power-Button (`GPIO4`) integriert:**
  * **Kurzer Druck:** Nächste OLED-Kachel bzw. Aufwecken aus dem Bildschirmschoner
  * **Doppelklick:** Sauberer System-Reboot (`systemctl reboot`)
  * **3 Sekunden Halten:** Sauberer Shutdown (`systemctl poweroff`)
* **100 % Standalone auf einem frischen Raspberry Pi OS:**
  * Benötigt **weder** das originale `argon1v5.sh`-Skript **noch** `python3-luma.oled` oder manuelles `raspi-config`.
  * Aktiviert bei der Installation automatisch `dtparam=i2c_arm=on` und lädt `i2c-dev` sofort zur Laufzeit (**Display startet sofort ohne Neustart**).
  * Richtet automatisch die vorderen USB-2.0-Anschlüsse (`dtoverlay=dwc2,dr_mode=host`), `usb_max_current_enable=1` sowie die Shutdown-Erkennung (`dtparam=uart0=on` + Systemd-Shutdown-Hook) ein.

### Schnellinstallation (Legacy Edition)
```bash
wget https://github.com/GamingTux/argon-v5-oled/releases/download/v1.0.0-legacy/argon-1v5-oled-legacy_1.0.0_arm64.deb
sudo apt install ./argon-1v5-oled-legacy_1.0.0_arm64.deb
```

### Konfiguration (Legacy Edition)
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
> **Experimenteller Alpha-Status:** Die Next-Gen Suite (`argon-v5-oled`) befindet sich in aktiver Entwicklung. Kachel-Layouts und der visuelle Editor zum Selbstgestalten eigener Kacheln werden laufend erweitert.

### Highlights in `v1.1.0-alpha`
* **Handoptimierte 1-Bit Terminus Bold Bitmap-Schrift:**
  Kristallklare, pixelperfekte 1-Bit-Darstellung ohne Anti-Aliasing-Fransen in drei Größen (`6×12`, `8×16`, `11×22`) – inklusive nativer UTF-8-Unterstützung für deutsche Umlaute und Sonderzeichen (`Ä Ö Ü ä ö ü ß ° ↑ ↓`).
* **Visueller Kachel-Designer (`argon-card-builder`):**
  128×64 WYSIWYG-Arbeitsfläche mit 4× Zoom, SVG/XBM-1-Bit-Konverter, Live-Vorschau mit der neuen Terminus-Schrift und `.argoncard`-Paketexport.
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

### Schnellinstallation (Experimentelle Next-Gen Suite)
```bash
wget https://github.com/GamingTux/argon-v5-oled/releases/download/v1.1.0-alpha/argon-v5-oled_1.1.0-alpha_arm64.deb
sudo apt install ./argon-v5-oled_1.1.0-alpha_arm64.deb
```

### CLI-Steuerung (`argon-oled`)
```bash
argon-oled sensors        # Scannt alle Hardware-Sensoren (Thermik, PMIC, GPU, RAID, USB)
argon-oled sensors --json # Maschinenlesbare JSON-Ausgabe
argon-oled next           # Nächste Kachel
argon-oled prev           # Vorherige Kachel
argon-oled screen 1       # Direkt zu Kachel 1 springen
argon-oled toggle         # Display an- / ausschalten
argon-oled dim            # Display dimmen
argon-oled status         # Status ausgeben
```

---

## 📂 Projektstruktur & Selbst-Kompilieren

```text
argon-v5-oled/
├── legacy/               (Stabiler Legacy C-Daemon, eingebettete Original-Bitmaps, argonone-oledconfig)
├── src/                  (Next-Gen C-Daemon & CLI mit Terminus Bold 6x12 / 8x16 / 11x22 Fonts)
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
