#!/usr/bin/env python3
"""
Argon ONE V5 - Harmonized 3-Size Font System Generator
Based on official system fonts (DejaVu Sans Bold & Liberation Sans Bold).
Generates full-coverage character sets (no missing glyphs, no fallback boxes)
and produces high-resolution comparison mockups.
"""

import os
from PIL import Image, ImageDraw, ImageFont

def render_font_test(font_path, thresh=100):
    f_small = ImageFont.truetype(font_path, 9)
    f_med = ImageFont.truetype(font_path, 13)
    f_large = ImageFont.truetype(font_path, 22)
    return f_small, f_med, f_large

def create_oled_screen(f_small, f_med, f_large, screen_type, thresh=95):
    """Render a 128x64 1-bit OLED screen using the harmonized 3 font sizes."""
    canvas = Image.new('L', (128, 64), 0)
    d = ImageDraw.Draw(canvas)

    if screen_type == "cpu":
        # Header Badge
        d.rectangle([1, 1, 26, 10], fill=255)
        d.text((3, 0), "CPU", font=f_small, fill=0)
        d.text((32, 0), "3.00 GHz", font=f_small, fill=255)
        d.line([1, 12, 126, 12], fill=255)

        # Hero Temp (Large)
        d.text((3, 14), "47.5°C", font=f_large, fill=255)

        # Fan in Medium
        d.text((80, 16), "3618", font=f_med, fill=255)
        d.text((80, 31), "RPM", font=f_small, fill=255)

        d.line([1, 45, 126, 45], fill=255)

        # Cores in Small
        for c in range(4):
            bx = 2 + c * 31
            d.text((bx, 48), f"C{c+1}", font=f_small, fill=255)
            d.rectangle([bx + 14, 51, bx + 27, 58], outline=255)
            d.rectangle([bx + 15, 52, bx + 22, 57], fill=255)

    elif screen_type == "clock":
        # Header
        d.rectangle([1, 1, 126, 10], fill=255)
        d.text((22, 0), "ARGON ONE V5", font=f_small, fill=0)

        # Hero Clock in Large
        d.text((26, 14), "23:08", font=f_large, fill=255)

        d.line([1, 44, 126, 44], fill=255)
        # Small Date with German umlauts
        d.text((12, 47), "Dienstag, 27. Okt 2026", font=f_small, fill=255)
        d.text((20, 55), "Uptime: 54d 13h 22m", font=f_small, fill=255)

    elif screen_type == "storage":
        # Header Badge
        d.rectangle([1, 1, 38, 10], fill=255)
        d.text((3, 0), "RAID 1", font=f_small, fill=0)
        d.text((43, 0), "[UU] HEALTHY", font=f_small, fill=255)
        d.text((100, 0), "33°C", font=f_small, fill=255)
        d.line([1, 12, 126, 12], fill=255)

        # Storage in Medium & Small
        d.text((3, 15), "42%", font=f_med, fill=255)
        d.text((38, 16), "480G / 1.0TB NVMe", font=f_small, fill=255)
        d.rectangle([3, 31, 124, 38], outline=255)
        d.rectangle([4, 32, 56, 37], fill=255)

        d.line([1, 42, 126, 42], fill=255)

        # RAM in Medium & Small
        d.text((3, 45), "RAM", font=f_med, fill=255)
        d.text((38, 47), "3.8G / 8.0G (48%)", font=f_small, fill=255)
        d.rectangle([38, 57, 124, 61], outline=255)
        d.rectangle([39, 58, 80, 60], fill=255)

    # Convert to 1-bit
    oled_1bit = canvas.point(lambda p: 255 if p >= thresh else 0, mode='1')

    # Scale 2x with cyan pixel styling
    scaled = Image.new('RGB', (256, 128), (8, 12, 18))
    sdraw = ImageDraw.Draw(scaled)
    for y in range(64):
        for x in range(128):
            if oled_1bit.getpixel((x, y)):
                sdraw.rectangle([x*2, y*2, x*2+1, y*2+1], fill=(0, 229, 255))
    return scaled

def build_comparison_sheet():
    width, height = 980, 840
    sheet = Image.new('RGB', (width, height), (13, 17, 23))
    draw = ImageDraw.Draw(sheet)

    try:
        sys_title = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf', 20)
        sys_sub = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf', 12)
        sys_sec = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf', 14)
        sys_text = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf', 11)
        sys_bold = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf', 12)
    except:
        sys_title = sys_sub = sys_sec = sys_text = sys_bold = ImageFont.load_default()

    # Header Card
    draw.rectangle([20, 20, width - 20, 85], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 30), "ARGON ONE V5 — HARMONISIERTES 3-STUFEN SCHRIFTENSYSTEM", fill=(0, 229, 255), font=sys_title)
    draw.text((36, 58), "100% konsistente Basis-Schriftart für alle Größen (Klein 9pt, Mittel 13pt, Groß 22pt) | Voller Zeichensatz ohne Lücken", fill=(139, 148, 158), font=sys_sub)

    # Load Base Font: DejaVu Sans Bold
    font_path = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
    f_small, f_med, f_large = render_font_test(font_path)

    # 1. Glyphs Overview Box
    draw.rectangle([20, 100, width - 20, 360], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 110), "SCHRIFTMUSTER-TABELLE: DEJAVU SANS BOLD (100% einheitliche Familie)", fill=(0, 229, 255), font=sys_sec)

    # Render Glyphs on L canvas then threshold
    c_w = width - 72
    glyph_canvas = Image.new('L', (c_w, 220), 0)
    gd = ImageDraw.Draw(glyph_canvas)

    # Section 1: Large (Hero)
    gd.text((0, 0), "1. GROSS (22pt Hero):", font=f_med, fill=160)
    gd.text((0, 18), "0 1 2 3 4 5 6 7 8 9   : . % ° C G M + -   47.5°C   23:08", font=f_large, fill=255)

    # Section 2: Medium (Header & Badges)
    gd.text((0, 60), "2. MITTEL (13pt Badges & Werte):", font=f_med, fill=160)
    gd.text((0, 78), "0 1 2 3 4 5 6 7 8 9   A B C D E F G H I K M N P R S T U V W X Y Z   : . % ° C M G B Hz [ ]", font=f_med, fill=255)
    gd.text((0, 96), "CPU 47.5°C   3618 RPM   1250 MHz   RAID 1 [UU]   NVMe 480G / 1.0TB (42%)", font=f_med, fill=255)

    # Section 3: Small (Micro & Full Text)
    gd.text((0, 130), "3. KLEIN (9pt Volltext & Metriken - Alle Buchstaben, Zahlen, Umlaute & Sonderzeichen):", font=f_med, fill=160)
    gd.text((0, 150), "Ziffern: 0 1 2 3 4 5 6 7 8 9  |  Groß: A B C D E F G H I J K L M N O P Q R S T U V W X Y Z", font=f_small, fill=255)
    gd.text((0, 166), "Klein: a b c d e f g h i j k l m n o p q r s t u v w x y z  |  Umlaute: Ä Ö Ü ä ö ü ß", font=f_small, fill=255)
    gd.text((0, 182), "Satzzeichen & Symbole: : . , ; / - _ + = % [ ] ( ) ! ? # @ °C  ↑ 14.2 MB/s  ↓ 52.8 MB/s", font=f_small, fill=255)
    gd.text((0, 198), "Beispiel: Dienstag, 27. Oktober 2026 | IP: 192.168.0.143 | Uptime: 54 Tage, 13 Stunden", font=f_small, fill=255)

    glyph_1bit = glyph_canvas.point(lambda p: 255 if p >= 90 else 0, mode='1')

    # Paste onto sheet with cyan tint
    glyph_rgb = Image.new('RGB', (c_w, 220), (22, 27, 34))
    gr_draw = ImageDraw.Draw(glyph_rgb)
    for y in range(220):
        for x in range(c_w):
            val = glyph_1bit.getpixel((x, y))
            if val:
                # Dim gray for labels, bright cyan for actual font text
                gr_draw.point((x, y), fill=(0, 229, 255))
    sheet.paste(glyph_rgb, (36, 130))

    # -------------------------------------------------------------------------
    # 2. Live OLED Screens
    # -------------------------------------------------------------------------
    draw.text((20, 380), "LIVE OLED-KACHELN (Zusammenspiel aller 3 Größen auf echter 128×64 Auflösung):", fill=(240, 246, 252), font=sys_sec)

    m_cpu = create_oled_screen(f_small, f_med, f_large, "cpu")
    m_clock = create_oled_screen(f_small, f_med, f_large, "clock")
    m_storage = create_oled_screen(f_small, f_med, f_large, "storage")

    x1, x2, x3 = 20, 345, 670
    y_m = 410

    sheet.paste(m_cpu, (x1, y_m))
    sheet.paste(m_clock, (x2, y_m))
    sheet.paste(m_storage, (x3, y_m))

    draw.rectangle([x1 - 2, y_m - 2, x1 + 258, y_m + 130], outline=(48, 54, 61), width=1)
    draw.rectangle([x2 - 2, y_m - 2, x2 + 258, y_m + 130], outline=(48, 54, 61), width=1)
    draw.rectangle([x3 - 2, y_m - 2, x3 + 258, y_m + 130], outline=(48, 54, 61), width=1)

    draw.text((x1, y_m + 136), "Kachel 1: CPU & Thermik", fill=(139, 148, 158), font=sys_bold)
    draw.text((x1, y_m + 152), "• Groß: 47.5°C Hero-Temp\n• Mittel: 3618 RPM\n• Klein: Takt & C1-C4 Cores", fill=(110, 118, 129), font=sys_text)

    draw.text((x2, y_m + 136), "Kachel 2: Hero-Uhr & Datum", fill=(139, 148, 158), font=sys_bold)
    draw.text((x2, y_m + 152), "• Groß: 23:08 Digitaluhr\n• Klein: Dienstag, 27. Okt\n• Klein: Uptime & Banner", fill=(110, 118, 129), font=sys_text)

    draw.text((x3, y_m + 136), "Kachel 3: RAID & Speicher", fill=(139, 148, 158), font=sys_bold)
    draw.text((x3, y_m + 152), "• Mittel: 42% & RAM\n• Klein: [UU] HEALTHY, 33°C\n• Klein: 480G / 1.0TB NVMe", fill=(110, 118, 129), font=sys_text)

    # -------------------------------------------------------------------------
    # 3. Quality & Comparison Details
    # -------------------------------------------------------------------------
    draw.rectangle([20, 620, width - 20, 810], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 630), "VORTEILE DIESES HARMONISIERTEN SYSTEMS (Gegenüber dem fehlerhaften Entwurf):", fill=(0, 229, 255), font=sys_sec)

    points = [
        ("100% Zeichenabdeckung (Keine Kästchen):", "Alle Kleinbuchstaben (a-z), Großbuchstaben (A-Z), deutsche Umlaute (ä ö ü ß) und Symbole (° ↑ ↓) sind vollständig vorhanden – keine 'Fallback-Kästchen' mehr!"),
        ("Gestochene Groß-Ziffern (Saubere Rundungen):", "Die 22pt Hero-Ziffern (0-9) haben saubere, exakte Strichstärken ohne Klumpen, Ausfransungen oder ungleiche Diagonalen."),
        ("Einheitliche Schrift-DNA:", "Alle 3 Größen stammen aus derselben Schriftfamilie. Die 3, 2, 8 und 0 haben in Klein, Mittel und Groß exakt denselben Schwung und dieselben Proportionen."),
        ("Tabellarische Monospace-Zahlen:", "Alle Ziffern teilen sich dieselbe Zeichenbreite – beim Hochzählen von 47.4° auf 47.5° oder der Uhr von 23:08 auf 23:09 zappelt nichts.")
    ]

    for idx, (head, desc) in enumerate(points):
        py = 658 + idx * 36
        draw.text((36, py), head, fill=(240, 246, 252), font=sys_bold)
        draw.text((36, py + 16), desc, fill=(139, 148, 158), font=sys_text)

    # Save outputs
    out_paths = [
        "/home/playx/.gemini/antigravity/brain/76f12623-5a2e-4fdd-826a-0df57c2ceb4e/argon_fonts_harmonized_specimen.png",
        "/home/playx/Downloads/argon-v5-oled/screenshots/font_specimen_harmonized.png"
    ]
    for p in out_paths:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        sheet.save(p, "PNG")
        print(f"Comparison sheet saved to {p}")

if __name__ == "__main__":
    build_comparison_sheet()
