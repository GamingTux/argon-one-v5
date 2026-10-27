#!/usr/bin/env python3
"""
Argon ONE V5 – Kachel-Designer & Screen Studio
Visual Drag-and-Drop / Coordinate Editor for 128x64 OLED displays.
With integrated SVG/XBM/Image Converter & portable .argoncard Package Bundler.
"""

import os
import sys
import json
import time
import zipfile
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from PIL import Image, ImageTk

# Ensure local imports work
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from oled_engine import (
    OledCanvas, ICONS_16X16, CUSTOM_ICONS,
    convert_svg_to_bitmap, convert_image_to_bitmap,
    parse_xbm_text, generate_xbm_text
)

# Theme Colors (GitHub Dark matching Argon OLED Control Center)
BG_DARK = "#0d1117"
BG_CARD = "#161b22"
BG_CARD_HOVER = "#21262d"
BORDER_COLOR = "#30363d"
ACCENT_CYAN = "#00e5ff"
ACCENT_BLUE = "#1f6feb"
ACCENT_GREEN = "#238636"
ACCENT_RED = "#da3633"
TEXT_PRIMARY = "#f0f6fc"
TEXT_SECONDARY = "#8b949e"

SHM_FB_PATH = "/dev/shm/argon_oled_fb.bin"

# Sample telemetry data for preview
SAMPLE_TELEMETRY = {
    "cpu_temp": 48.5,
    "cpu_freq": "3.00 GHz",
    "cpu_usage": 42,
    "fan_rpm": 5400,
    "nvme_pct": 84,
    "ram_pct": 11,
    "usb_pct": 42,
    "ip_addr": "192.168.178.132",
    "net_down": "12.4M/s",
    "net_up": "840K/s",
    "clock": "09:21",
    "seconds": ":45",
    "date": "Di, 27. Okt",
    "uptime": "03h 45m"
}

# Pre-defined templates
TEMPLATES = {
    "Blank (Leere Kachel)": [],
    "Vorlage 1: CPU, Temp & Lüfter": [
        {"type": "icon", "name": "processor", "x": 2, "y": 1, "color": 1},
        {"type": "text", "text": "CPU SYSTEM", "font": "small", "x": 22, "y": 4, "color": 1},
        {"type": "sensor", "key": "cpu_freq", "font": "small", "x": 76, "y": 4, "color": 1},
        {"type": "line", "x0": 1, "y0": 16, "x1": 126, "y1": 16, "color": 1},
        {"type": "sensor", "key": "cpu_temp", "font": "large", "format": "{val:.1f}^C", "x": 8, "y": 22, "color": 1},
        {"type": "line", "x0": 82, "y0": 19, "x1": 82, "y1": 48, "color": 1},
        {"type": "icon", "name": "computer_fan", "x": 95, "y": 18, "color": 1},
        {"type": "sensor", "key": "fan_rpm", "font": "small", "format": "{val}", "x": 89, "y": 36, "color": 1},
        {"type": "text", "text": "RPM", "font": "small", "x": 92, "y": 45, "color": 1},
        {"type": "line", "x0": 1, "y0": 51, "x1": 126, "y1": 51, "color": 1},
        {"type": "bar", "key": "cpu_usage", "x": 16, "y": 54, "w": 40, "h": 8, "color": 1},
        {"type": "text", "text": "C1", "font": "small", "x": 2, "y": 54, "color": 1},
        {"type": "bar", "key": "cpu_usage", "x": 78, "y": 54, "w": 40, "h": 8, "color": 1},
        {"type": "text", "text": "C2", "font": "small", "x": 64, "y": 54, "color": 1}
    ],
    "Vorlage 2: Speicher & HiFi-Equalizer": [
        {"type": "rect", "x": 1, "y": 1, "w": 126, "h": 11, "fill": True, "color": 1},
        {"type": "text", "text": "STORAGE & MEMORY", "font": "small", "x": 16, "y": 2, "color": 0},
        {"type": "icon", "name": "ssd", "x": 2, "y": 13, "color": 1},
        {"type": "bar", "key": "nvme_pct", "x": 22, "y": 16, "w": 38, "h": 9, "color": 1},
        {"type": "eq", "x": 65, "y": 16, "h": 9, "bands": [3, 6, 8, 7, 5, 3, 2], "peaks": [4, 8, 9, 9, 7, 5, 3]},
        {"type": "sensor", "key": "nvme_pct", "font": "med", "format": "{val}%", "x": 97, "y": 15, "color": 1},
        {"type": "line", "x0": 1, "y0": 30, "x1": 126, "y1": 30, "color": 1},
        {"type": "icon", "name": "ram_alt", "x": 2, "y": 32, "color": 1},
        {"type": "bar", "key": "ram_pct", "x": 22, "y": 34, "w": 38, "h": 9, "color": 1},
        {"type": "eq", "x": 65, "y": 34, "h": 9, "bands": [2, 3, 4, 3, 2, 2, 1], "peaks": [3, 5, 6, 5, 3, 2, 1]},
        {"type": "sensor", "key": "ram_pct", "font": "med", "format": "{val}%", "x": 97, "y": 33, "color": 1},
        {"type": "line", "x0": 1, "y0": 46, "x1": 126, "y1": 46, "color": 1},
        {"type": "icon", "name": "hdd", "x": 2, "y": 48, "color": 1},
        {"type": "bar", "key": "usb_pct", "x": 22, "y": 50, "w": 38, "h": 9, "color": 1},
        {"type": "eq", "x": 65, "y": 50, "h": 9, "bands": [1, 2, 5, 6, 4, 2, 1], "peaks": [2, 4, 7, 8, 6, 3, 1]},
        {"type": "sensor", "key": "usb_pct", "font": "med", "format": "{val}%", "x": 97, "y": 49, "color": 1}
    ],
    "Vorlage 3: Netzwerk & IP Banner": [
        {"type": "icon", "name": "wi_fi_alt", "x": 2, "y": 1, "color": 1},
        {"type": "text", "text": "WLAN (1000M)", "font": "small", "x": 22, "y": 4, "color": 1},
        {"type": "text", "text": "[1/2]", "font": "small", "x": 94, "y": 4, "color": 1},
        {"type": "rect", "x": 1, "y": 18, "w": 126, "h": 15, "fill": True, "color": 1},
        {"type": "sensor", "key": "ip_addr", "font": "med", "x": 18, "y": 19, "color": 0},
        {"type": "icon", "name": "arrow_down_alt", "x": 2, "y": 33, "color": 1},
        {"type": "sensor", "key": "net_down", "font": "small", "x": 18, "y": 37, "color": 1},
        {"type": "icon", "name": "arrow_up_alt", "x": 66, "y": 33, "color": 1},
        {"type": "sensor", "key": "net_up", "font": "small", "x": 82, "y": 37, "color": 1},
        {"type": "line", "x0": 1, "y0": 49, "x1": 126, "y1": 49, "color": 1},
        {"type": "sparkline", "x": 1, "y": 51, "w": 126, "h": 11, "data": [1, 2, 4, 8, 14, 22, 18, 12, 6, 2, 3, 9, 16, 24, 20, 15, 8, 3, 2, 5, 12, 19, 21]}
    ],
    "Vorlage 4: Große Uhrzeit & Heute": [
        {"type": "icon", "name": "clock", "x": 2, "y": 1, "color": 1},
        {"type": "text", "text": "TODAY", "font": "small", "x": 22, "y": 4, "color": 1},
        {"type": "sensor", "key": "date", "font": "small", "x": 58, "y": 4, "color": 1},
        {"type": "line", "x0": 1, "y0": 16, "x1": 126, "y1": 16, "color": 1},
        {"type": "sensor", "key": "clock", "font": "large", "x": 22, "y": 22, "color": 1},
        {"type": "sensor", "key": "seconds", "font": "med", "x": 85, "y": 23, "color": 1},
        {"type": "line", "x0": 1, "y0": 45, "x1": 126, "y1": 45, "color": 1},
        {"type": "icon", "name": "lightning_bolt", "x": 2, "y": 47, "color": 1},
        {"type": "text", "text": "UPTIME: 03h 45m", "font": "small", "x": 19, "y": 52, "color": 1},
        {"type": "icon", "name": "thermometer_alt", "x": 88, "y": 47, "color": 1},
        {"type": "sensor", "key": "cpu_temp", "font": "small", "format": "{val:.0f}C", "x": 104, "y": 52, "color": 1}
    ]
}

# ==============================================================================
# ICON IMPORT CONVERTER DIALOG (SVG / XBM / IMAGE TO 1-BIT)
# ==============================================================================
class IconImportDialog(tk.Toplevel):
    def __init__(self, parent, on_icon_imported):
        super().__init__(parent)
        self.title("📥 Icon-Konverter (SVG / XBM / Bild zu 1-Bit)")
        self.configure(bg=BG_DARK)
        self.geometry("480x460")
        self.resizable(False, False)
        self.transient(parent)
        self.grab_set()

        self.on_icon_imported = on_icon_imported
        self.src_file = None
        self.preview_photo = None
        self.last_result = None

        self.build_ui()

    def build_ui(self):
        # File selector top
        top_f = tk.Frame(self, bg=BG_CARD, bd=1, relief=tk.SOLID)
        top_f.pack(fill=tk.X, padx=12, pady=10)

        tk.Label(top_f, text="Grafikdatei:", bg=BG_CARD, fg=TEXT_SECONDARY, font=("sans-serif", 9)).pack(side=tk.LEFT, padx=10, pady=8)
        self.lbl_file = tk.Label(top_f, text="Keine Datei ausgewählt", bg=BG_CARD, fg=TEXT_PRIMARY, font=("sans-serif", 9, "bold"))
        self.lbl_file.pack(side=tk.LEFT, padx=4, expand=True, fill=tk.X)

        tk.Button(top_f, text="Durchsuchen...", bg=ACCENT_BLUE, fg="#ffffff", activebackground="#1158c7",
                  bd=0, padx=10, pady=4, cursor="hand2", command=self.pick_file).pack(side=tk.RIGHT, padx=10)

        # Settings
        set_box = tk.LabelFrame(self, text=" Konvertierungs-Einstellungen ", bg=BG_CARD, fg=ACCENT_CYAN, font=("sans-serif", 9, "bold"), bd=1, relief=tk.SOLID)
        set_box.pack(fill=tk.X, padx=12, pady=4)

        grid = tk.Frame(set_box, bg=BG_CARD)
        grid.pack(fill=tk.X, padx=10, pady=8)

        # Name
        tk.Label(grid, text="Icon-Name:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=0, column=0, sticky="w", pady=4)
        self.ent_name = tk.Entry(grid, bg="#21262d", fg=TEXT_PRIMARY, insertbackground=TEXT_PRIMARY, width=22)
        self.ent_name.grid(row=0, column=1, columnspan=2, sticky="w", pady=4)

        # Size
        tk.Label(grid, text="Zielgröße:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=1, column=0, sticky="w", pady=4)
        size_f = tk.Frame(grid, bg=BG_CARD)
        size_f.grid(row=1, column=1, columnspan=2, sticky="w", pady=4)

        self.sp_w = tk.Spinbox(size_f, from_=8, to=64, width=4, bg="#21262d", fg=TEXT_PRIMARY, command=self.update_preview)
        self.sp_w.delete(0, tk.END)
        self.sp_w.insert(0, "16")
        self.sp_w.pack(side=tk.LEFT)
        self.sp_w.bind("<KeyRelease>", lambda e: self.update_preview())

        tk.Label(size_f, text="×", bg=BG_CARD, fg=TEXT_SECONDARY, padx=4).pack(side=tk.LEFT)

        self.sp_h = tk.Spinbox(size_f, from_=8, to=64, width=4, bg="#21262d", fg=TEXT_PRIMARY, command=self.update_preview)
        self.sp_h.delete(0, tk.END)
        self.sp_h.insert(0, "16")
        self.sp_h.pack(side=tk.LEFT)
        self.sp_h.bind("<KeyRelease>", lambda e: self.update_preview())

        tk.Button(size_f, text="16×16", font=("sans-serif", 8), bg="#21262d", fg=TEXT_PRIMARY, bd=0, padx=6, pady=2,
                  command=lambda: self.set_size(16, 16)).pack(side=tk.LEFT, padx=(8, 2))
        tk.Button(size_f, text="24×24", font=("sans-serif", 8), bg="#21262d", fg=TEXT_PRIMARY, bd=0, padx=6, pady=2,
                  command=lambda: self.set_size(24, 24)).pack(side=tk.LEFT, padx=2)

        # Threshold slider
        tk.Label(grid, text="Schwellenwert:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=2, column=0, sticky="w", pady=4)
        self.scale_th = tk.Scale(grid, from_=10, to=240, orient=tk.HORIZONTAL, bg=BG_CARD, fg=TEXT_PRIMARY,
                                 highlightthickness=0, troughcolor="#21262d", command=lambda v: self.update_preview())
        self.scale_th.set(100)
        self.scale_th.grid(row=2, column=1, columnspan=2, sticky="ew", pady=4)

        # Invert Checkbox
        self.var_inv = tk.BooleanVar(value=False)
        tk.Checkbutton(grid, text="Farben umkehren (Invertieren)", variable=self.var_inv, bg=BG_CARD, fg=TEXT_PRIMARY,
                       activebackground=BG_CARD, selectcolor="#21262d", command=self.update_preview).grid(row=3, column=1, sticky="w", pady=2)

        # Preview Area
        prev_box = tk.LabelFrame(self, text=" 1-Bit OLED Vorschau ", bg=BG_CARD, fg=TEXT_PRIMARY, font=("sans-serif", 9), bd=1, relief=tk.SOLID)
        prev_box.pack(fill=tk.BOTH, expand=True, padx=12, pady=6)

        self.lbl_prev = tk.Label(prev_box, bg="#04070b", bd=2, relief=tk.SOLID)
        self.lbl_prev.pack(pady=8)

        # Footer Buttons
        foot_f = tk.Frame(self, bg=BG_DARK)
        foot_f.pack(fill=tk.X, padx=12, pady=(0, 10))

        tk.Button(foot_f, text="Abbrechen", font=("sans-serif", 9), bg="#21262d", fg=TEXT_PRIMARY,
                  bd=0, padx=12, pady=4, cursor="hand2", command=self.destroy).pack(side=tk.LEFT)

        tk.Button(foot_f, text="💾 Als XBM speichern...", font=("sans-serif", 9), bg="#21262d", fg=TEXT_PRIMARY,
                  bd=0, padx=12, pady=4, cursor="hand2", command=self.export_xbm_file).pack(side=tk.LEFT, padx=6)

        tk.Button(foot_f, text="✓ Als Icon übernehmen", font=("sans-serif", 9, "bold"), bg=ACCENT_GREEN, fg="#ffffff",
                  activebackground="#2ea043", bd=0, padx=16, pady=4, cursor="hand2", command=self.apply_icon).pack(side=tk.RIGHT)

    def set_size(self, w, h):
        self.sp_w.delete(0, tk.END)
        self.sp_w.insert(0, str(w))
        self.sp_h.delete(0, tk.END)
        self.sp_h.insert(0, str(h))
        self.update_preview()

    def pick_file(self):
        path = filedialog.askopenfilename(filetypes=[
            ("Alle unterstützten Grafiken", "*.svg *.xbm *.png *.jpg *.jpeg *.bmp"),
            ("Vektorgrafiken (SVG)", "*.svg"),
            ("X-Bitmaps (XBM)", "*.xbm"),
            ("Rastergrafiken (PNG, JPG)", "*.png *.jpg *.bmp")
        ])
        if path:
            self.src_file = path
            bname = os.path.basename(path)
            self.lbl_file.config(text=bname)
            # Pre-fill name
            clean_name = os.path.splitext(bname)[0].lower()
            clean_name = "".join(c if c.isalnum() else "_" for c in clean_name)
            self.ent_name.delete(0, tk.END)
            self.ent_name.insert(0, clean_name)
            self.update_preview()

    def update_preview(self):
        if not self.src_file:
            return
        try:
            w = int(self.sp_w.get())
            h = int(self.sp_h.get())
            inv = self.var_inv.get()
            th = self.scale_th.get()

            ext = os.path.splitext(self.src_file)[1].lower()
            if ext == ".xbm":
                with open(self.src_file, "r") as fp:
                    text = fp.read()
                name, w, h, msb_bytes, xbm_bytes = parse_xbm_text(text, self.ent_name.get())
                xbm_text = text
                self.sp_w.delete(0, tk.END)
                self.sp_w.insert(0, str(w))
                self.sp_h.delete(0, tk.END)
                self.sp_h.insert(0, str(h))
            elif ext == ".svg":
                name, w, h, msb_bytes, xbm_bytes, xbm_text = convert_svg_to_bitmap(self.src_file, w, h, inv, th)
            else:
                name, w, h, msb_bytes, xbm_bytes, xbm_text = convert_image_to_bitmap(self.src_file, w, h, inv, th)

            self.last_result = {
                "name": self.ent_name.get() or name,
                "w": w, "h": h,
                "msb_bytes": msb_bytes,
                "xbm_bytes": xbm_bytes,
                "xbm_text": xbm_text
            }

            # Create scaled 1-bit PIL preview image
            SCALE = 5
            prev_img = Image.new("RGB", (w * SCALE, h * SCALE), (9, 18, 28))
            pixels = prev_img.load()
            row_bytes = (w + 7) // 8
            for r in range(h):
                for c in range(w):
                    byte_idx = r * row_bytes + (c // 8)
                    bit = 7 - (c % 8)
                    if (msb_bytes[byte_idx] >> bit) & 1:
                        for dy in range(SCALE - 1):
                            for dx in range(SCALE - 1):
                                pixels[c * SCALE + dx, r * SCALE + dy] = (0, 229, 255)

            self.preview_photo = ImageTk.PhotoImage(prev_img)
            self.lbl_prev.config(image=self.preview_photo)

        except Exception as e:
            print("Preview error:", e)

    def export_xbm_file(self):
        if not self.last_result:
            messagebox.showwarning("Hinweis", "Bitte wähle zuerst eine Grafikdatei aus.")
            return
        dest = filedialog.asksaveasfilename(defaultextension=".xbm", filetypes=[("XBM Datei", "*.xbm")])
        if dest:
            try:
                with open(dest, "w") as fp:
                    fp.write(self.last_result["xbm_text"])
                messagebox.showinfo("Exportiert", f"XBM-Datei gespeichert:\n{dest}")
            except Exception as e:
                messagebox.showerror("Fehler", str(e))

    def apply_icon(self):
        if not self.last_result:
            messagebox.showwarning("Hinweis", "Bitte wähle zuerst eine Grafikdatei aus.")
            return
        name = self.ent_name.get().strip() or self.last_result["name"]
        self.last_result["name"] = name
        self.on_icon_imported(self.last_result)
        self.destroy()

# ==============================================================================
# MAIN CARD BUILDER APPLICATION
# ==============================================================================
class CardBuilderApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Argon ONE V5 – Kachel-Designer & Screen Studio")
        self.configure(bg=BG_DARK)
        self.geometry("1020x700")
        self.minsize(920, 620)

        self.elements = []
        self.selected_idx = -1
        self.current_photo = None
        self.show_grid = tk.BooleanVar(value=True)
        self.use_live_telemetry = tk.BooleanVar(value=False)

        self.setup_styles()
        self.build_ui()
        self.load_template("Vorlage 1: CPU, Temp & Lüfter")

    def setup_styles(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure(".", background=BG_DARK, foreground=TEXT_PRIMARY, font=("sans-serif", 9))
        style.configure("TFrame", background=BG_DARK)
        style.configure("Card.TFrame", background=BG_CARD, relief="solid", borderwidth=1)
        style.configure("TLabel", background=BG_DARK, foreground=TEXT_PRIMARY)
        style.configure("Header.TLabel", background=BG_CARD, foreground=ACCENT_CYAN, font=("sans-serif", 10, "bold"))
        style.configure("Sub.TLabel", background=BG_CARD, foreground=TEXT_SECONDARY, font=("sans-serif", 8))
        style.configure("Treeview", background="#1c2128", foreground=TEXT_PRIMARY, fieldbackground="#1c2128", borderwidth=0)
        style.map("Treeview", background=[("selected", ACCENT_BLUE)])

    def build_ui(self):
        # ---------------- TOP HEADER ----------------
        top_bar = tk.Frame(self, bg=BG_CARD, bd=1, relief=tk.SOLID)
        top_bar.pack(fill=tk.X, padx=10, pady=(10, 6))

        tk.Label(top_bar, text="🎨 KACHEL-DESIGNER (128×64 OLED)", font=("sans-serif", 11, "bold"),
                 bg=BG_CARD, fg=ACCENT_CYAN).pack(side=tk.LEFT, padx=12, pady=8)

        # Template Selector
        tk.Label(top_bar, text="Vorlage:", bg=BG_CARD, fg=TEXT_SECONDARY).pack(side=tk.LEFT, padx=(10, 4))
        self.combo_tpl = ttk.Combobox(top_bar, values=list(TEMPLATES.keys()), state="readonly", width=26)
        self.combo_tpl.current(1)
        self.combo_tpl.pack(side=tk.LEFT, padx=4)
        self.combo_tpl.bind("<<ComboboxSelected>>", lambda e: self.load_template(self.combo_tpl.get()))

        # Live OLED Push
        tk.Button(top_bar, text="🚀 Auf OLED spiegeln", font=("sans-serif", 9, "bold"),
                  bg=ACCENT_BLUE, fg="#ffffff", activebackground="#1158c7", bd=0, padx=12, pady=4,
                  cursor="hand2", command=self.push_to_oled).pack(side=tk.RIGHT, padx=10)

        # Package Export / Import (.argoncard)
        tk.Button(top_bar, text="📦 Paket exportieren", font=("sans-serif", 9, "bold"),
                  bg=ACCENT_GREEN, fg="#ffffff", activebackground="#2ea043", bd=0, padx=10, pady=4,
                  cursor="hand2", command=self.export_argoncard).pack(side=tk.RIGHT, padx=4)

        tk.Button(top_bar, text="📂 Paket laden", font=("sans-serif", 9),
                  bg="#21262d", fg=TEXT_PRIMARY, activebackground="#30363d", bd=0, padx=10, pady=4,
                  cursor="hand2", command=self.import_argoncard).pack(side=tk.RIGHT, padx=4)

        # ---------------- MAIN 2-COLUMN SPLIT ----------------
        main_split = tk.Frame(self, bg=BG_DARK)
        main_split.pack(fill=tk.BOTH, expand=True, padx=10, pady=4)

        # LEFT: CANVAS & ADD BUTTONS
        left_pane = tk.LabelFrame(main_split, text=" OLED 128×64 ARBEITSFLÄCHE (4× ZOOM) ",
                                  font=("sans-serif", 9, "bold"), bg=BG_CARD, fg=ACCENT_CYAN, bd=1, relief=tk.SOLID)
        left_pane.pack(side=tk.LEFT, fill=tk.BOTH, padx=(0, 6), pady=4)

        # Canvas Frame (512 x 256)
        self.canvas_lbl = tk.Label(left_pane, bg="#04070b", bd=2, relief=tk.SOLID, cursor="crosshair")
        self.canvas_lbl.pack(padx=14, pady=10)
        self.canvas_lbl.bind("<Button-1>", self.on_canvas_click)
        self.canvas_lbl.bind("<Motion>", self.on_canvas_hover)

        # Coordinate & info bar under canvas
        coord_bar = tk.Frame(left_pane, bg=BG_CARD)
        coord_bar.pack(fill=tk.X, padx=14, pady=(0, 6))

        self.coord_lbl = tk.Label(coord_bar, text="Cursor: X=-- , Y=-- | 1px-Sicherheitsrand: X=1..126, Y=1..62",
                                  font=("monospace", 8), bg=BG_CARD, fg=TEXT_SECONDARY)
        self.coord_lbl.pack(side=tk.LEFT)

        # Quick Add Toolbox
        tool_box = tk.LabelFrame(left_pane, text=" Baustein hinzufügen ", bg=BG_CARD, fg=TEXT_PRIMARY, font=("sans-serif", 9))
        tool_box.pack(fill=tk.X, padx=10, pady=(6, 10))

        btn_row1 = tk.Frame(tool_box, bg=BG_CARD)
        btn_row1.pack(fill=tk.X, padx=6, pady=4)

        btn_kw = {"bg": "#21262d", "fg": TEXT_PRIMARY, "activebackground": "#30363d",
                  "bd": 0, "padx": 8, "pady": 4, "font": ("sans-serif", 8, "bold"), "cursor": "hand2"}

        tk.Button(btn_row1, text="➕ Text", command=lambda: self.add_element("text"), **btn_kw).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        tk.Button(btn_row1, text="➕ Sensor-Wert", command=lambda: self.add_element("sensor"), **btn_kw).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        tk.Button(btn_row1, text="➕ Icon", command=lambda: self.add_element("icon"), **btn_kw).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        tk.Button(btn_row1, text="📥 SVG/XBM Import...", command=self.open_icon_import_dialog,
                  bg=ACCENT_BLUE, fg="#ffffff", activebackground="#1158c7", bd=0, padx=8, pady=4,
                  font=("sans-serif", 8, "bold"), cursor="hand2").pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)

        btn_row2 = tk.Frame(tool_box, bg=BG_CARD)
        btn_row2.pack(fill=tk.X, padx=6, pady=(0, 6))

        tk.Button(btn_row2, text="➕ Balken", command=lambda: self.add_element("bar"), **btn_kw).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        tk.Button(btn_row2, text="➕ Box / Fläche", command=lambda: self.add_element("rect"), **btn_kw).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        tk.Button(btn_row2, text="➕ Trennlinie", command=lambda: self.add_element("line"), **btn_kw).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        tk.Button(btn_row2, text="➕ HiFi-EQ", command=lambda: self.add_element("eq"), **btn_kw).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)

        # RIGHT: LAYER LIST & PROPERTY INSPECTOR
        right_pane = tk.Frame(main_split, bg=BG_DARK)
        right_pane.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(6, 0), pady=4)

        # Layers Box
        layers_box = tk.LabelFrame(right_pane, text=" KACHEL-ELEMENTE (EBENEN) ",
                                   font=("sans-serif", 9, "bold"), bg=BG_CARD, fg=TEXT_PRIMARY, bd=1, relief=tk.SOLID)
        layers_box.pack(fill=tk.BOTH, expand=True, pady=(0, 6))

        cols = ("typ", "pos", "info")
        self.tree = ttk.Treeview(layers_box, columns=cols, show="headings", height=7, selectmode="browse")
        self.tree.heading("typ", text="Typ")
        self.tree.heading("pos", text="Pos (X, Y)")
        self.tree.heading("info", text="Details / Inhalt")
        self.tree.column("typ", width=75, stretch=False)
        self.tree.column("pos", width=85, stretch=False)
        self.tree.column("info", width=160, stretch=True)
        self.tree.pack(fill=tk.BOTH, expand=True, padx=8, pady=6)
        self.tree.bind("<<TreeviewSelect>>", self.on_layer_selected)

        # Layer buttons
        l_btn_bar = tk.Frame(layers_box, bg=BG_CARD)
        l_btn_bar.pack(fill=tk.X, padx=8, pady=(0, 6))

        tk.Button(l_btn_bar, text="▲ Rauf", font=("sans-serif", 8), bg="#21262d", fg=TEXT_PRIMARY,
                  bd=0, padx=6, pady=2, cursor="hand2", command=self.move_up).pack(side=tk.LEFT, padx=2)
        tk.Button(l_btn_bar, text="▼ Runter", font=("sans-serif", 8), bg="#21262d", fg=TEXT_PRIMARY,
                  bd=0, padx=6, pady=2, cursor="hand2", command=self.move_down).pack(side=tk.LEFT, padx=2)
        tk.Button(l_btn_bar, text="📋 Kopieren", font=("sans-serif", 8), bg="#21262d", fg=TEXT_PRIMARY,
                  bd=0, padx=6, pady=2, cursor="hand2", command=self.duplicate_element).pack(side=tk.LEFT, padx=2)
        tk.Button(l_btn_bar, text="🗑 Löschen", font=("sans-serif", 8), bg="#21262d", fg=ACCENT_RED,
                  bd=0, padx=6, pady=2, cursor="hand2", command=self.delete_element).pack(side=tk.RIGHT, padx=2)

        # Properties Inspector Box
        self.prop_box = tk.LabelFrame(right_pane, text=" EIGENSCHAFTEN AUSGEWÄHLTES ELEMENT ",
                                      font=("sans-serif", 9, "bold"), bg=BG_CARD, fg=ACCENT_CYAN, bd=1, relief=tk.SOLID)
        self.prop_box.pack(fill=tk.X, pady=(4, 0))

        self.build_property_fields(self.prop_box)

    def build_property_fields(self, parent):
        grid_f = tk.Frame(parent, bg=BG_CARD)
        grid_f.pack(fill=tk.X, padx=10, pady=8)

        # Row 0: Coordinates X, Y
        tk.Label(grid_f, text="X:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=0, column=0, sticky="w", pady=2)
        self.sp_x = tk.Spinbox(grid_f, from_=0, to=127, width=5, bg="#21262d", fg=TEXT_PRIMARY,
                               command=self.on_prop_changed)
        self.sp_x.grid(row=0, column=1, sticky="w", padx=(2, 10))
        self.sp_x.bind("<KeyRelease>", lambda e: self.on_prop_changed())

        tk.Label(grid_f, text="Y:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=0, column=2, sticky="w", pady=2)
        self.sp_y = tk.Spinbox(grid_f, from_=0, to=63, width=5, bg="#21262d", fg=TEXT_PRIMARY,
                               command=self.on_prop_changed)
        self.sp_y.grid(row=0, column=3, sticky="w", padx=(2, 10))
        self.sp_y.bind("<KeyRelease>", lambda e: self.on_prop_changed())

        # Width & Height
        tk.Label(grid_f, text="Breite W:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=0, column=4, sticky="w", pady=2)
        self.sp_w = tk.Spinbox(grid_f, from_=1, to=128, width=5, bg="#21262d", fg=TEXT_PRIMARY,
                               command=self.on_prop_changed)
        self.sp_w.grid(row=0, column=5, sticky="w", padx=(2, 10))
        self.sp_w.bind("<KeyRelease>", lambda e: self.on_prop_changed())

        tk.Label(grid_f, text="Höhe H:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=0, column=6, sticky="w", pady=2)
        self.sp_h = tk.Spinbox(grid_f, from_=1, to=64, width=5, bg="#21262d", fg=TEXT_PRIMARY,
                               command=self.on_prop_changed)
        self.sp_h.grid(row=0, column=7, sticky="w", padx=(2, 4))
        self.sp_h.bind("<KeyRelease>", lambda e: self.on_prop_changed())

        # Row 1: Font Size & Color Invert
        tk.Label(grid_f, text="Schrift:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=1, column=0, sticky="w", pady=4)
        self.combo_font = ttk.Combobox(grid_f, values=["small (Terminus 6×12)", "med (Terminus 8×16)", "large (Terminus 11×22 Hero)"],
                                       state="readonly", width=16)
        self.combo_font.current(0)
        self.combo_font.grid(row=1, column=1, columnspan=3, sticky="w", pady=4)
        self.combo_font.bind("<<ComboboxSelected>>", lambda e: self.on_prop_changed())

        self.var_color = tk.IntVar(value=1)
        self.chk_invert = tk.Checkbutton(grid_f, text="Farbe: Weiß (1)", variable=self.var_color,
                                         bg=BG_CARD, fg=TEXT_PRIMARY, activebackground=BG_CARD,
                                         selectcolor="#21262d", command=self.on_prop_changed)
        self.chk_invert.grid(row=1, column=4, columnspan=4, sticky="w", padx=4)

        # Row 2: Sensor / Text / Icon value
        tk.Label(grid_f, text="Inhalt / Sensor:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=2, column=0, sticky="w", pady=4)
        self.ent_val = tk.Entry(grid_f, bg="#21262d", fg=TEXT_PRIMARY, insertbackground=TEXT_PRIMARY, width=22)
        self.ent_val.grid(row=2, column=1, columnspan=4, sticky="ew", pady=4)
        self.ent_val.bind("<KeyRelease>", lambda e: self.on_prop_changed())

        # Sensor Helper Dropdown
        self.combo_sensor = ttk.Combobox(grid_f, values=[
            "cpu_temp", "cpu_freq", "cpu_usage", "fan_rpm", "nvme_pct", "ram_pct",
            "usb_pct", "ip_addr", "net_down", "net_up", "clock", "seconds", "date", "uptime"
        ], state="readonly", width=12)
        self.combo_sensor.grid(row=2, column=5, columnspan=3, sticky="w", padx=(4, 0))
        self.combo_sensor.bind("<<ComboboxSelected>>", self.on_sensor_selected)

        # Row 3: Icon Dropdown & Import Button
        tk.Label(grid_f, text="Icon:", bg=BG_CARD, fg=TEXT_SECONDARY).grid(row=3, column=0, sticky="w", pady=4)
        self.combo_icon = ttk.Combobox(grid_f, values=self.get_all_icon_names(), state="readonly", width=18)
        self.combo_icon.grid(row=3, column=1, columnspan=3, sticky="w", pady=4)
        self.combo_icon.bind("<<ComboboxSelected>>", self.on_icon_selected)

        tk.Button(grid_f, text="📥 Neu...", font=("sans-serif", 8), bg=ACCENT_BLUE, fg="#ffffff",
                  activebackground="#1158c7", bd=0, padx=8, pady=2, cursor="hand2",
                  command=self.open_icon_import_dialog).grid(row=3, column=4, columnspan=2, sticky="w", padx=(4, 0))

    def get_all_icon_names(self):
        names = list(ICONS_16X16.keys())
        for cname in CUSTOM_ICONS.keys():
            if cname not in names:
                names.append(cname)
        return names

    def open_icon_import_dialog(self):
        IconImportDialog(self, self.on_custom_icon_registered)

    def on_custom_icon_registered(self, icon_data):
        name = icon_data["name"]
        CUSTOM_ICONS[name] = icon_data
        all_icons = self.get_all_icon_names()
        self.combo_icon.config(values=all_icons)
        self.combo_icon.set(name)
        if 0 <= self.selected_idx < len(self.elements):
            el = self.elements[self.selected_idx]
            if el.get("type") == "icon":
                el["name"] = name
                self.render_preview()
                self.refresh_layer_list()

    def on_sensor_selected(self, event=None):
        sens = self.combo_sensor.get()
        if sens and 0 <= self.selected_idx < len(self.elements):
            el = self.elements[self.selected_idx]
            if el.get("type") in ("sensor", "bar", "eq"):
                el["key"] = sens
                self.ent_val.delete(0, tk.END)
                self.ent_val.insert(0, sens)
                self.render_preview()
                self.refresh_layer_list()

    def on_icon_selected(self, event=None):
        icon = self.combo_icon.get()
        if icon and 0 <= self.selected_idx < len(self.elements):
            el = self.elements[self.selected_idx]
            if el.get("type") == "icon":
                el["name"] = icon
                self.render_preview()
                self.refresh_layer_list()

    def on_canvas_click(self, event):
        x = event.x // 4
        y = event.y // 4
        if 0 <= self.selected_idx < len(self.elements):
            el = self.elements[self.selected_idx]
            el["x"] = x
            el["y"] = y
            self.sp_x.delete(0, tk.END)
            self.sp_x.insert(0, str(x))
            self.sp_y.delete(0, tk.END)
            self.sp_y.insert(0, str(y))
            self.render_preview()
            self.refresh_layer_list()

    def on_canvas_hover(self, event):
        x = event.x // 4
        y = event.y // 4
        warn = " ⚠️ Randkontakt!" if (x == 0 or x == 127 or y == 0 or y == 63) else " (Sicher)"
        self.coord_lbl.config(text=f"Cursor: X={x:2d}, Y={y:2d} {warn}")

    def on_layer_selected(self, event=None):
        sel = self.tree.selection()
        if not sel: return
        idx = int(sel[0])
        if 0 <= idx < len(self.elements):
            self.selected_idx = idx
            el = self.elements[idx]
            self.load_element_into_props(el)

    def load_element_into_props(self, el):
        t = el.get("type", "text")
        self.sp_x.delete(0, tk.END)
        self.sp_x.insert(0, str(el.get("x", el.get("x0", 0))))
        self.sp_y.delete(0, tk.END)
        self.sp_y.insert(0, str(el.get("y", el.get("y0", 0))))

        self.sp_w.delete(0, tk.END)
        self.sp_w.insert(0, str(el.get("w", 20)))
        self.sp_h.delete(0, tk.END)
        self.sp_h.insert(0, str(el.get("h", 10)))

        f = el.get("font", "small")
        if f == "small": self.combo_font.current(0)
        elif f == "med": self.combo_font.current(1)
        elif f == "large": self.combo_font.current(2)

        c = el.get("color", 1)
        self.var_color.set(c)
        self.chk_invert.config(text=f"Farbe: {'Weiß (1)' if c == 1 else 'Schwarz (0)'}")

        val = el.get("text", el.get("key", el.get("name", "")))
        self.ent_val.delete(0, tk.END)
        self.ent_val.insert(0, str(val))

        all_names = self.get_all_icon_names()
        self.combo_icon.config(values=all_names)
        if "name" in el and el["name"] in all_names:
            self.combo_icon.set(el["name"])
        if "key" in el:
            self.combo_sensor.set(el["key"])

    def on_prop_changed(self):
        if not (0 <= self.selected_idx < len(self.elements)):
            return
        el = self.elements[self.selected_idx]
        try:
            el["x"] = int(self.sp_x.get())
            el["y"] = int(self.sp_y.get())
        except ValueError:
            pass

        t = el.get("type")
        if t in ("rect", "bar", "sparkline"):
            try:
                el["w"] = int(self.sp_w.get())
                el["h"] = int(self.sp_h.get())
            except ValueError:
                pass

        font_map = ["small", "med", "large"]
        el["font"] = font_map[self.combo_font.current()]
        el["color"] = self.var_color.get()
        self.chk_invert.config(text=f"Farbe: {'Weiß (1)' if el['color'] == 1 else 'Schwarz (0)'}")

        v = self.ent_val.get()
        if t == "text":
            el["text"] = v
        elif t in ("sensor", "bar", "eq"):
            el["key"] = v
        elif t == "icon":
            el["name"] = v

        self.render_preview()
        self.refresh_layer_list()

    def add_element(self, elem_type):
        new_el = {"type": elem_type, "color": 1}
        if elem_type == "text":
            new_el.update({"text": "NEUER TEXT", "font": "small", "x": 10, "y": 20})
        elif elem_type == "sensor":
            new_el.update({"key": "cpu_temp", "font": "med", "format": "{val:.1f}C", "x": 10, "y": 20})
        elif elem_type == "icon":
            new_el.update({"name": "processor", "x": 2, "y": 14})
        elif elem_type == "bar":
            new_el.update({"key": "ram_pct", "x": 20, "y": 20, "w": 50, "h": 9})
        elif elem_type == "rect":
            new_el.update({"x": 1, "y": 1, "w": 126, "h": 12, "fill": True, "color": 1})
        elif elem_type == "line":
            new_el.update({"x0": 1, "y0": 30, "x1": 126, "y1": 30, "color": 1})
        elif elem_type == "eq":
            new_el.update({"x": 60, "y": 20, "h": 9, "bands": [2, 5, 8, 6, 3], "peaks": [3, 6, 8, 7, 4]})

        self.elements.append(new_el)
        self.selected_idx = len(self.elements) - 1
        self.refresh_layer_list()
        self.load_element_into_props(new_el)
        self.render_preview()

    def delete_element(self):
        if 0 <= self.selected_idx < len(self.elements):
            del self.elements[self.selected_idx]
            self.selected_idx = max(0, self.selected_idx - 1) if self.elements else -1
            self.refresh_layer_list()
            self.render_preview()

    def duplicate_element(self):
        if 0 <= self.selected_idx < len(self.elements):
            import copy
            cp = copy.deepcopy(self.elements[self.selected_idx])
            if "x" in cp: cp["x"] = min(120, cp["x"] + 4)
            if "y" in cp: cp["y"] = min(56, cp["y"] + 4)
            self.elements.append(cp)
            self.selected_idx = len(self.elements) - 1
            self.refresh_layer_list()
            self.render_preview()

    def move_up(self):
        if self.selected_idx > 0:
            self.elements[self.selected_idx], self.elements[self.selected_idx - 1] = \
                self.elements[self.selected_idx - 1], self.elements[self.selected_idx]
            self.selected_idx -= 1
            self.refresh_layer_list()
            self.render_preview()

    def move_down(self):
        if 0 <= self.selected_idx < len(self.elements) - 1:
            self.elements[self.selected_idx], self.elements[self.selected_idx + 1] = \
                self.elements[self.selected_idx + 1], self.elements[self.selected_idx]
            self.selected_idx += 1
            self.refresh_layer_list()
            self.render_preview()

    def refresh_layer_list(self):
        for item in self.tree.get_children():
            self.tree.delete(item)

        for idx, el in enumerate(self.elements):
            t = el.get("type", "item").upper()
            x = el.get("x", el.get("x0", 0))
            y = el.get("y", el.get("y0", 0))
            pos = f"X:{x:2d}, Y:{y:2d}"

            info = ""
            if t == "TEXT": info = f'"{el.get("text", "")}" ({el.get("font", "small")})'
            elif t == "SENSOR": info = f'Sensor: {el.get("key", "")} ({el.get("font", "small")})'
            elif t == "ICON": info = f'Icon: {el.get("name", "")}'
            elif t == "RECT": info = f'Box: {el.get("w", 0)}×{el.get("h", 0)}px (Fill: {el.get("fill", False)})'
            elif t == "LINE": info = f'Linie: ({el.get("x0")},{el.get("y0")}) -> ({el.get("x1")},{el.get("y1")})'
            elif t == "BAR": info = f'Balken: {el.get("key", "")} ({el.get("w", 0)}px)'
            elif t == "EQ": info = f'HiFi-EQ: {len(el.get("bands", []))} Bänder'
            elif t == "SPARKLINE": info = 'Sparkline 60s'

            self.tree.insert("", tk.END, iid=str(idx), values=(t, pos, info))

        if 0 <= self.selected_idx < len(self.elements):
            self.tree.selection_set(str(self.selected_idx))

    def render_preview(self):
        cv = OledCanvas()

        for el in self.elements:
            t = el.get("type")
            c = el.get("color", 1)

            if t == "rect":
                cv.rect(el.get("x", 0), el.get("y", 0), el.get("w", 10), el.get("h", 10),
                        fill=el.get("fill", False), color=c)
            elif t == "line":
                cv.line(el.get("x0", 0), el.get("y0", 0), el.get("x1", 127), el.get("y1", 0), color=c)
            elif t == "text":
                txt = el.get("text", "")
                f = el.get("font", "small")
                x, y = el.get("x", 0), el.get("y", 0)
                if f == "small": cv.draw_string_small(x, y, txt, c)
                elif f == "med": cv.draw_string_med(x, y, txt, c)
                elif f == "large": cv.draw_string_large(x, y, txt, c)
            elif t == "sensor":
                k = el.get("key", "cpu_temp")
                val = SAMPLE_TELEMETRY.get(k, 0)
                fmt = el.get("format")
                if fmt:
                    try:
                        disp_str = fmt.format(val=val)
                    except Exception:
                        disp_str = str(val)
                else:
                    disp_str = str(val)
                f = el.get("font", "small")
                x, y = el.get("x", 0), el.get("y", 0)
                if f == "small": cv.draw_string_small(x, y, disp_str, c)
                elif f == "med": cv.draw_string_med(x, y, disp_str, c)
                elif f == "large": cv.draw_string_large(x, y, disp_str, c)
            elif t == "icon":
                cv.draw_icon(el.get("x", 0), el.get("y", 0), el.get("name", "processor"), c)
            elif t == "bar":
                k = el.get("key", "ram_pct")
                val = float(SAMPLE_TELEMETRY.get(k, 50.0))
                cv.draw_bar(el.get("x", 0), el.get("y", 0), el.get("w", 40), el.get("h", 8), val)
            elif t == "eq":
                bands = el.get("bands", [2, 4, 6, 8, 5, 3])
                peaks = el.get("peaks", [3, 5, 7, 9, 6, 4])
                cv.draw_equalizer(el.get("x", 0), el.get("y", 0), el.get("h", 9), bands, peaks)
            elif t == "sparkline":
                data = el.get("data", [1, 3, 7, 15, 20, 16, 8, 4, 2])
                cv.draw_sparkline(el.get("x", 0), el.get("y", 0), el.get("w", 126), el.get("h", 12), data)

        # Highlight currently selected element on preview
        if 0 <= self.selected_idx < len(self.elements):
            sel_el = self.elements[self.selected_idx]
            sx = sel_el.get("x", sel_el.get("x0", 0))
            sy = sel_el.get("y", sel_el.get("y0", 0))
            sw = sel_el.get("w", 16)
            sh = sel_el.get("h", 16)
            # Invert 4 corner indicator pixels
            cv.pixel(sx, sy, 1 - cv.get_pixel(sx, sy))
            cv.pixel(sx + sw - 1, sy, 1 - cv.get_pixel(sx + sw - 1, sy))
            cv.pixel(sx, sy + sh - 1, 1 - cv.get_pixel(sx, sy + sh - 1))
            cv.pixel(sx + sw - 1, sy + sh - 1, 1 - cv.get_pixel(sx + sw - 1, sy + sh - 1))

        self.last_canvas = cv
        img = cv.to_pil_image(scale=4)
        self.current_photo = ImageTk.PhotoImage(img)
        self.canvas_lbl.config(image=self.current_photo)

    def load_template(self, tpl_name):
        import copy
        if tpl_name in TEMPLATES:
            self.elements = copy.deepcopy(TEMPLATES[tpl_name])
            self.selected_idx = 0 if self.elements else -1
            self.refresh_layer_list()
            if self.elements:
                self.load_element_into_props(self.elements[0])
            self.render_preview()

    def push_to_oled(self):
        if not hasattr(self, "last_canvas") or not self.last_canvas:
            return
        buf = self.last_canvas.to_ssd1306_bytes()
        try:
            with open(SHM_FB_PATH, "wb") as f:
                f.write(buf)
            messagebox.showinfo("OLED Übertragen", "Kachel wurde erfolgreich live auf das echte Display übertragen!")
        except Exception as e:
            messagebox.showerror("Fehler", f"Konnte nicht auf {SHM_FB_PATH} schreiben:\n{e}")

    def export_argoncard(self):
        dest = filedialog.asksaveasfilename(defaultextension=".argoncard", filetypes=[
            ("Argon Kachel-Paket", "*.argoncard"),
            ("ZIP Archiv", "*.zip")
        ])
        if not dest:
            return
        try:
            with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as zf:
                # 1. card.json
                data = {
                    "format": "argon-card-package",
                    "version": "1.0",
                    "name": os.path.splitext(os.path.basename(dest))[0],
                    "elements": self.elements
                }
                zf.writestr("card.json", json.dumps(data, indent=2))

                # 2. icons/*.xbm for all used custom icons
                used_icons = set()
                for el in self.elements:
                    if el.get("type") == "icon" and el.get("name") in CUSTOM_ICONS:
                        used_icons.add(el["name"])

                for iname in used_icons:
                    cdata = CUSTOM_ICONS[iname]
                    zf.writestr(f"icons/{iname}.xbm", cdata.get("xbm_text", ""))

                # 3. preview.png
                if hasattr(self, "last_canvas") and self.last_canvas:
                    p_img = self.last_canvas.to_pil_image(scale=4)
                    import io
                    p_buf = io.BytesIO()
                    p_img.save(p_buf, format="PNG")
                    zf.writestr("preview.png", p_buf.getvalue())

            messagebox.showinfo("Paket Exportiert", f"Kachel-Paket erfolgreich gespeichert!\n{dest}\n\nAlle verwendeten XBM-Grafiken wurden automatisch mit eingepackt.")
        except Exception as e:
            messagebox.showerror("Fehler beim Export", str(e))

    def import_argoncard(self):
        src = filedialog.askopenfilename(filetypes=[
            ("Alle Kachel-Dateien", "*.argoncard *.zip *.json"),
            ("Argon Kachel-Paket", "*.argoncard"),
            ("ZIP Kachel-Archiv", "*.zip"),
            ("Einzelne JSON Datei", "*.json")
        ])
        if not src:
            return
        try:
            if src.endswith(".json"):
                with open(src, "r", encoding="utf-8") as fp:
                    data = json.load(fp)
                    self.elements = data.get("elements", [])
            else:
                # Zip / .argoncard bundle
                with zipfile.ZipFile(src, "r") as zf:
                    # Load icons
                    for name in zf.namelist():
                        if name.startswith("icons/") and name.endswith(".xbm"):
                            xbm_text = zf.read(name).decode("utf-8")
                            iname, w, h, msb_bytes, xbm_bytes = parse_xbm_text(xbm_text)
                            CUSTOM_ICONS[iname] = {
                                "name": iname, "w": w, "h": h,
                                "msb_bytes": msb_bytes, "xbm_bytes": xbm_bytes,
                                "xbm_text": xbm_text
                            }

                    # Load card.json
                    card_raw = zf.read("card.json").decode("utf-8")
                    data = json.loads(card_raw)
                    self.elements = data.get("elements", [])

            self.combo_icon.config(values=self.get_all_icon_names())
            self.selected_idx = 0 if self.elements else -1
            self.refresh_layer_list()
            if self.elements:
                self.load_element_into_props(self.elements[0])
            self.render_preview()
            messagebox.showinfo("Geladen", f"Kachel erfolgreich importiert:\n{os.path.basename(src)}")
        except Exception as e:
            messagebox.showerror("Fehler beim Import", str(e))

if __name__ == "__main__":
    app = CardBuilderApp()
    app.mainloop()
