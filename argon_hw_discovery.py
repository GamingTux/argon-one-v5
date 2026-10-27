#!/usr/bin/env python3
"""
Argon ONE V5 - Hardware & Sensor Discovery Engine
Scans the system for all standard and expansion sensors on Raspberry Pi 5.
"""

import os
import sys
import glob
import json
import subprocess
import argparse

def run_cmd(cmd):
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=2)
        return res.stdout.strip()
    except Exception:
        return ""

def scan_thermals():
    thermals = []
    # Scan hwmon
    for hpath in sorted(glob.glob("/sys/class/hwmon/hwmon*")):
        name_file = os.path.join(hpath, "name")
        hw_name = ""
        if os.path.exists(name_file):
            with open(name_file, "r") as f:
                hw_name = f.read().strip()

        # Find all temperature inputs in this hwmon
        for temp_input in sorted(glob.glob(os.path.join(hpath, "temp*_input"))):
            base = temp_input[:-6] # strip _input
            label_file = base + "_label"
            crit_file = base + "_crit"
            label = ""
            if os.path.exists(label_file):
                with open(label_file, "r") as f:
                    label = f.read().strip()
            
            crit_val = None
            if os.path.exists(crit_file):
                try:
                    with open(crit_file, "r") as f:
                        crit_val = float(f.read().strip()) / 1000.0
                except:
                    pass

            try:
                with open(temp_input, "r") as f:
                    mdeg = float(f.read().strip())
                    deg = mdeg / 1000.0
            except:
                continue

            display_name = hw_name
            if hw_name == "cpu_thermal":
                display_name = "SoC (CPU / GPU)"
            elif hw_name == "rp1_adc":
                display_name = "RP1 Southbridge"
            elif hw_name == "nvme":
                display_name = f"NVMe SSD ({label if label else os.path.basename(base)})"

            thermals.append({
                "driver": hw_name,
                "label": label if label else os.path.basename(base),
                "name": display_name,
                "path": temp_input,
                "temp_c": round(deg, 1),
                "crit_c": crit_val
            })
    return thermals

def scan_cooling():
    cooling = {
        "fan_rpm": None,
        "fan_rpm_path": None,
        "pwm_val": None,
        "cur_state": None,
        "max_state": None,
        "trip_points": []
    }
    # Fan RPM & PWM
    for hpath in sorted(glob.glob("/sys/class/hwmon/hwmon*")):
        fan_input = os.path.join(hpath, "fan1_input")
        if os.path.exists(fan_input):
            try:
                with open(fan_input, "r") as f:
                    cooling["fan_rpm"] = int(f.read().strip())
                    cooling["fan_rpm_path"] = fan_input
            except:
                pass
        pwm_input = os.path.join(hpath, "pwm1")
        if os.path.exists(pwm_input):
            try:
                with open(pwm_input, "r") as f:
                    cooling["pwm_val"] = int(f.read().strip())
            except:
                pass

    # Cooling device
    cur_st = "/sys/class/thermal/cooling_device0/cur_state"
    max_st = "/sys/class/thermal/cooling_device0/max_state"
    if os.path.exists(cur_st):
        try:
            with open(cur_st, "r") as f:
                cooling["cur_state"] = int(f.read().strip())
            with open(max_st, "r") as f:
                cooling["max_state"] = int(f.read().strip())
        except:
            pass

    # Trip points
    for tp in sorted(glob.glob("/sys/class/thermal/thermal_zone0/trip_point_*_temp")):
        try:
            with open(tp, "r") as f:
                t_val = int(f.read().strip()) // 1000
                cooling["trip_points"].append(t_val)
        except:
            pass
    cooling["trip_points"].sort()
    return cooling

def scan_pmic_and_power():
    power = {
        "in_5v_volt": None,
        "core_volt": None,
        "core_curr_a": None,
        "core_power_w": None,
        "ram_volt": None,
        "ram_curr_a": None,
        "battery_volt": None,
        "undervoltage_alarm": False,
        "throttled_raw": None
    }
    # Check hwmon rpi_volt
    for hpath in sorted(glob.glob("/sys/class/hwmon/hwmon*")):
        alarm_file = os.path.join(hpath, "in0_lcrit_alarm")
        if os.path.exists(alarm_file):
            try:
                with open(alarm_file, "r") as f:
                    power["undervoltage_alarm"] = (int(f.read().strip()) != 0)
            except:
                pass

    # vcgencmd throttled
    th_out = run_cmd("vcgencmd get_throttled")
    if "throttled=" in th_out:
        power["throttled_raw"] = th_out.split("=")[1].strip()
        # 0x1 = under-voltage now, 0x50000 = under-voltage occurred since boot
        try:
            val = int(power["throttled_raw"], 16)
            if (val & 0x1) or (val & 0x10000):
                power["undervoltage_alarm"] = True
        except:
            pass

    # vcgencmd pmic_read_adc
    pmic_out = run_cmd("vcgencmd pmic_read_adc")
    if pmic_out:
        for line in pmic_out.splitlines():
            line = line.strip()
            if "EXT5V_V" in line and "volt(" in line:
                try:
                    power["in_5v_volt"] = round(float(line.split("=")[1].replace("V", "")), 2)
                except: pass
            elif "VDD_CORE_V" in line and "volt(" in line:
                try:
                    power["core_volt"] = round(float(line.split("=")[1].replace("V", "")), 3)
                except: pass
            elif "VDD_CORE_A" in line and "current(" in line:
                try:
                    power["core_curr_a"] = round(float(line.split("=")[1].replace("A", "")), 3)
                except: pass
            elif "DDR_VDD2_V" in line and "volt(" in line:
                try:
                    power["ram_volt"] = round(float(line.split("=")[1].replace("V", "")), 2)
                except: pass
            elif "DDR_VDD2_A" in line and "current(" in line:
                try:
                    power["ram_curr_a"] = round(float(line.split("=")[1].replace("A", "")), 3)
                except: pass
            elif "BATT_V" in line and "volt(" in line:
                try:
                    power["battery_volt"] = round(float(line.split("=")[1].replace("V", "")), 3)
                except: pass

    if power["core_volt"] is not None and power["core_curr_a"] is not None:
        power["core_power_w"] = round(power["core_volt"] * power["core_curr_a"], 2)

    return power

def scan_gpu():
    gpu = {
        "model": "VideoCore VII",
        "has_gpu_stats": False,
        "v3d_clock_mhz": None,
        "core_clock_mhz": None,
        "gpu_mem_fw": None
    }
    stats_path = "/sys/class/drm/card0/device/gpu_stats"
    if os.path.exists(stats_path):
        gpu["has_gpu_stats"] = True
        gpu["gpu_stats_path"] = stats_path

    v3d_out = run_cmd("vcgencmd measure_clock v3d")
    if "frequency(" in v3d_out:
        try:
            hz = int(v3d_out.split("=")[1])
            gpu["v3d_clock_mhz"] = round(hz / 1_000_000)
        except: pass

    core_out = run_cmd("vcgencmd measure_clock core")
    if "frequency(" in core_out:
        try:
            hz = int(core_out.split("=")[1])
            gpu["core_clock_mhz"] = round(hz / 1_000_000)
        except: pass

    mem_out = run_cmd("vcgencmd get_mem gpu")
    if "gpu=" in mem_out:
        gpu["gpu_mem_fw"] = mem_out.split("=")[1].strip()

    return gpu

def scan_storage():
    storage = {
        "nvme_drives": [],
        "raid_arrays": [],
        "microsd": {
            "present": False,
            "device": "/dev/mmcblk0",
            "size_gb": 0,
            "mounted_at": []
        },
        "usb_drives": []
    }

    # NVMe Drives
    for npath in sorted(glob.glob("/sys/class/nvme/nvme*")):
        nvme_name = os.path.basename(npath)
        model_file = os.path.join(npath, "model")
        model = "Generic NVMe"
        if os.path.exists(model_file):
            with open(model_file, "r") as f:
                model = f.read().strip()

        # Check block devices for this nvme
        block_nodes = glob.glob(f"/sys/block/{nvme_name}n*")
        for bnode in block_nodes:
            bname = os.path.basename(bnode)
            size_file = os.path.join(bnode, "size")
            size_gb = 0
            if os.path.exists(size_file):
                try:
                    with open(size_file, "r") as f:
                        sectors = int(f.read().strip())
                        size_gb = round((sectors * 512) / (1024**3), 1)
                except: pass

            storage["nvme_drives"].append({
                "controller": nvme_name,
                "device": f"/dev/{bname}",
                "model": model,
                "size_gb": size_gb
            })

    # Software RAID (/proc/mdstat)
    if os.path.exists("/proc/mdstat"):
        with open("/proc/mdstat", "r") as f:
            lines = f.readlines()
        curr_md = None
        for line in lines:
            parts = line.strip().split()
            if len(parts) >= 3 and parts[0].startswith("md"):
                curr_md = {
                    "name": parts[0],
                    "state": parts[1], # active/inactive
                    "level": parts[2], # raid1, raid0, raid5
                    "members": parts[3:],
                    "health": "OK",
                    "degraded": False
                }
                storage["raid_arrays"].append(curr_md)
            elif curr_md and "[" in line and "]" in line:
                # e.g. [2/2] [UU] or [2/1] [U_]
                if "[U_]" in line or "[_U]" in line or "_" in line:
                    curr_md["health"] = "DEGRADED"
                    curr_md["degraded"] = True
                curr_md["status_line"] = line.strip()

    # MicroSD Check
    sd_size_path = "/sys/block/mmcblk0/size"
    if os.path.exists(sd_size_path):
        try:
            with open(sd_size_path, "r") as f:
                sec = int(f.read().strip())
                if sec > 0:
                    storage["microsd"]["present"] = True
                    storage["microsd"]["size_gb"] = round((sec * 512) / (1024**3), 1)
        except: pass

    # USB Mass Storage Drives (/dev/sd*)
    for sdpath in sorted(glob.glob("/sys/block/sd*")):
        sdname = os.path.basename(sdpath)
        # Check if USB
        target = ""
        try:
            target = os.readlink(os.path.join(sdpath, "device"))
        except: pass

        model_file = os.path.join(sdpath, "device/model")
        vendor_file = os.path.join(sdpath, "device/vendor")
        vendor = ""
        model = ""
        if os.path.exists(vendor_file):
            with open(vendor_file, "r") as f: vendor = f.read().strip()
        if os.path.exists(model_file):
            with open(model_file, "r") as f: model = f.read().strip()

        size_file = os.path.join(sdpath, "size")
        size_gb = 0
        if os.path.exists(size_file):
            try:
                with open(size_file, "r") as f:
                    sec = int(f.read().strip())
                    size_gb = round((sec * 512) / (1024**3), 1)
            except: pass

        storage["usb_drives"].append({
            "device": f"/dev/{sdname}",
            "vendor": vendor,
            "model": model if model else "USB Storage",
            "size_gb": size_gb,
            "is_usb": ("/usb" in target)
        })

    return storage

def scan_connectivity():
    conn = {
        "ethernet": [],
        "wifi": [],
        "bluetooth": {
            "present": False,
            "powered": False,
            "connected_devices": []
        },
        "zigbee": {
            "adapter_found": False,
            "port": None,
            "chip": None
        }
    }

    # Ethernet & WiFi
    for ifpath in sorted(glob.glob("/sys/class/net/*")):
        ifname = os.path.basename(ifpath)
        if ifname == "lo": continue
        is_wireless = os.path.exists(os.path.join(ifpath, "wireless")) or os.path.exists(os.path.join(ifpath, "phy80211"))
        operstate = ""
        try:
            with open(os.path.join(ifpath, "operstate"), "r") as f:
                operstate = f.read().strip()
        except: pass

        ip_addr = run_cmd(f"ip -4 addr show {ifname} | grep -oP '(?<=inet\\s)\\d+(\\.\\d+){{3}}'").split("\n")[0]

        if is_wireless:
            ssid = run_cmd(f"iwgetid -r {ifname}")
            conn["wifi"].append({
                "interface": ifname,
                "state": operstate,
                "ip": ip_addr,
                "ssid": ssid
            })
        else:
            speed = ""
            speed_file = os.path.join(ifpath, "speed")
            if os.path.exists(speed_file):
                try:
                    with open(speed_file, "r") as f:
                        s = int(f.read().strip())
                        if s > 0: speed = f"{s} Mbps"
                except: pass
            conn["ethernet"].append({
                "interface": ifname,
                "state": operstate,
                "ip": ip_addr,
                "speed": speed
            })

    # Bluetooth (hci0)
    if os.path.exists("/sys/class/bluetooth/hci0"):
        conn["bluetooth"]["present"] = True
        rf_state = run_cmd("rfkill list bluetooth")
        conn["bluetooth"]["powered"] = ("Soft blocked: no" in rf_state)
        # Check connected devices via bluetoothctl
        bt_devs = run_cmd("bluetoothctl devices Connected")
        if bt_devs:
            for l in bt_devs.splitlines():
                if "Device" in l:
                    conn["bluetooth"]["connected_devices"].append(l.strip())

    # Zigbee Discovery (USB / UART Serial devices)
    serial_devs = glob.glob("/dev/serial/by-id/*")
    for sdev in serial_devs:
        target = os.path.realpath(sdev)
        sname = os.path.basename(sdev).lower()
        if any(keyword in sname for keyword in ["zigbee", "silicon_labs", "efr32", "conbee", "sonoff", "ch340", "cp210"]):
            conn["zigbee"]["adapter_found"] = True
            conn["zigbee"]["port"] = target
            conn["zigbee"]["chip"] = os.path.basename(sdev)
            break

    # Also check /dev/ttyAMA*
    if not conn["zigbee"]["adapter_found"]:
        for ama in ["/dev/ttyAMA0", "/dev/ttyAMA1", "/dev/ttyACM0"]:
            if os.path.exists(ama):
                conn["zigbee"]["port_candidate"] = ama

    return conn

def print_pretty_report(data):
    print("=" * 64)
    print("  ARGON ONE V5 - SYSTEM & SENSOR HARDWARE DISCOVERY")
    print("=" * 64)

    # 1. Thermals
    print("\n🌡️  TEMPERATUR-SENSOREN (hwmon & thermal)")
    print("-" * 64)
    for t in data["thermals"]:
        crit_str = f" [Crit: {t['crit_c']}°C]" if t["crit_c"] else ""
        print(f"  • {t['name']:<30} : {t['temp_c']:>5.1f} °C {crit_str}")

    # 2. Cooling
    c = data["cooling"]
    print("\n❄️  LÜFTER & KÜHLUNG")
    print("-" * 64)
    rpm_str = f"{c['fan_rpm']} RPM" if c['fan_rpm'] is not None else "Kein Tacho"
    pwm_str = f"PWM {c['pwm_val']}/255" if c['pwm_val'] is not None else "N/A"
    st_str = f"Stufe {c['cur_state']} von {c['max_state']}" if c['cur_state'] is not None else "N/A"
    print(f"  • Lüfter-Tachometer           : {rpm_str} ({pwm_str}, {st_str})")
    if c["trip_points"]:
        print(f"  • Active-Cooler Schwellen      : {'°C -> '.join(map(str, c['trip_points']))}°C")

    # 3. PMIC / Power
    p = data["power"]
    print("\n⚡ STROMVERSORGUNG & PMIC")
    print("-" * 64)
    in5v = f"{p['in_5v_volt']} V" if p['in_5v_volt'] else "N/A"
    watt = f"{p['core_power_w']} W" if p['core_power_w'] else "N/A"
    c_volt = f"{p['core_volt']} V" if p['core_volt'] else "N/A"
    c_curr = f"{p['core_curr_a']} A" if p['core_curr_a'] else "N/A"
    uv_status = "⚠️ UNTERSPANNUNG ERKANNT!" if p["undervoltage_alarm"] else "✅ Stabil (Kein Brownout)"
    print(f"  • Eingangsspannung (USB-C 5V) : {in5v}")
    print(f"  • Netzteil-Status              : {uv_status}")
    print(f"  • SoC Kernspannung & Strom    : {c_volt} @ {c_curr} ({watt})")
    if p["ram_volt"]:
        print(f"  • LPDDR RAM Versorgung         : {p['ram_volt']} V @ {p['ram_curr_a']} A")
    if p["battery_volt"] is not None:
        batt_str = f"{p['battery_volt']} V" if p['battery_volt'] > 0.5 else "Keine Batterie gesteckt"
        print(f"  • RTC Backup-Batterie         : {batt_str}")

    # 4. GPU & CPU
    g = data["gpu"]
    print("\n🎮 GRAFIK & RECHENLEISTUNG (VideoCore VII & CPU)")
    print("-" * 64)
    v3d_str = f"{g['v3d_clock_mhz']} MHz" if g['v3d_clock_mhz'] else "N/A"
    core_str = f"{g['core_clock_mhz']} MHz" if g['core_clock_mhz'] else "N/A"
    stats_str = "✅ Hardware-Queues (/sys/.../gpu_stats)" if g['has_gpu_stats'] else "Nicht verfügbar"
    print(f"  • GPU Modell                  : {g['model']}")
    print(f"  • GPU 3D / Core Takt          : {v3d_str} (3D Engine) / {core_str} (Display Core)")
    print(f"  • GPU Last-Messung            : {stats_str}")

    # 5. Storage Matrix
    st = data["storage"]
    print("\n💾 SPEICHER-HIERARCHIE (NVMe, RAID, SD, USB)")
    print("-" * 64)
    if st["nvme_drives"]:
        for n in st["nvme_drives"]:
            print(f"  • NVMe Slot ({n['controller']})      : {n['device']} [{n['model']}] ({n['size_gb']} GB)")
    else:
        print("  • NVMe Slots                  : Keine NVMe SSD gefunden")

    if st["raid_arrays"]:
        for r in st["raid_arrays"]:
            status_icon = "✅" if not r["degraded"] else "⚠️"
            print(f"  • RAID Array ({r['name']})        : {status_icon} {r['level'].upper()} ({r['health']})")
    else:
        print("  • Software RAID               : Kein aktives RAID konfiguriert")

    sd = st["microsd"]
    sd_str = f"Eingesteckt ({sd['size_gb']} GB)" if sd["present"] else "Slot leer"
    print(f"  • MicroSD-Karten-Slot         : {sd_str}")

    if st["usb_drives"]:
        for u in st["usb_drives"]:
            print(f"  • USB-Laufwerk                : {u['device']} [{u['vendor']} {u['model']}] ({u['size_gb']} GB)")
    else:
        print("  • USB-Laufwerke               : Keine externen Datenträger angeschlossen")

    # 6. Connectivity & Smart Home
    co = data["connectivity"]
    print("\n📡 KONNEKTIVITÄT & SMART HOME (Netzwerk, BT, Zigbee)")
    print("-" * 64)
    for eth in co["ethernet"]:
        print(f"  • Ethernet ({eth['interface']})          : {eth['state'].upper()} | IP: {eth['ip'] if eth['ip'] else 'Keine'} ({eth['speed']})")
    for wf in co["wifi"]:
        print(f"  • WLAN ({wf['interface']})              : {wf['state'].upper()} | SSID: '{wf['ssid']}' | IP: {wf['ip'] if wf['ip'] else 'Keine'}")
    
    bt = co["bluetooth"]
    bt_str = "Aktiv" if (bt["present"] and bt["powered"]) else ("Aus" if bt["present"] else "Nicht vorhanden")
    print(f"  • Bluetooth                   : {bt_str} ({len(bt['connected_devices'])} Geräte verbunden)")

    zg = co["zigbee"]
    if zg["adapter_found"]:
        print(f"  • Zigbee Modul / Dongle       : ✅ Gefunden an {zg['port']} ({zg['chip']})")
    elif "port_candidate" in zg:
        print(f"  • Zigbee / Seriell Port       : Serieller Port vorhanden ({zg['port_candidate']})")
    else:
        print(f"  • Zigbee Modul                : Kein aktives Funkmodul erkannt")

    print("=" * 64)

def main():
    parser = argparse.ArgumentParser(description="Argon ONE V5 Sensor & Hardware Discovery")
    parser.add_argument("--json", action="store_true", help="Output machine-readable JSON")
    args = parser.parse_args()

    data = {
        "thermals": scan_thermals(),
        "cooling": scan_cooling(),
        "power": scan_pmic_and_power(),
        "gpu": scan_gpu(),
        "storage": scan_storage(),
        "connectivity": scan_connectivity()
    }

    if args.json:
        print(json.dumps(data, indent=2))
    else:
        print_pretty_report(data)

if __name__ == "__main__":
    main()
