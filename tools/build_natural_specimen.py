#!/usr/bin/env python3
"""
Argon ONE V5 - Natural Pure Typography Specimen (NO 45° Chamfer / Pure Sans)
Renders Liberation Sans Bold in its natural, unadulterated form:
Pure vertical stems, natural clean curves, flat baselines, zero artificial corner cuts.
"""

import os
from PIL import Image, ImageDraw, ImageFont

def render_natural_specimen():
    font_path = '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf'

    spec_w, spec_h = 960, 840
    sheet = Image.new('RGB', (spec_w, spec_h), (13, 17, 23))
    draw = ImageDraw.Draw(sheet)

    try:
        sys_title = ImageFont.truetype(font_path, 20)
        sys_sub = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf', 12)
        sys_sec = ImageFont.truetype(font_path, 14)
        sys_bold = ImageFont.truetype(font_path, 12)
        sys_text = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf', 11)
    except:
        sys_title = sys_sub = sys_sec = sys_bold = sys_text = ImageFont.load_default()

    # Header Card
    draw.rectangle([20, 20, spec_w - 20, 85], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 30), "ARGON ONE V5 — NATÜRLICHE TYPOGRAFIE (OHNE 45°-FEATURE)", fill=(0, 229, 255), font=sys_title)
    draw.text((36, 58), "Reine Liberation Sans Bold | Natürliche Kurven, gerade Stämme, harmonische Proportionen, 0% künstliche Ecken", fill=(139, 148, 158), font=sys_sub)

    # 4x Oversampling setup for ultra-clean natural 1-bit rasterization
    SCALE = 4
    f_s_hi = ImageFont.truetype(font_path, 10 * SCALE)
    f_m_hi = ImageFont.truetype(font_path, 14 * SCALE)
    f_l_hi = ImageFont.truetype(font_path, 22 * SCALE)

    # 1. Glyphs Overview Box
    draw.rectangle([20, 100, spec_w - 20, 360], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 110), "VOLLSTÄNDIGER ZEICHENSATZ (NATÜRLICHE FORM OHNE FASEN)", fill=(0, 229, 255), font=sys_sec)

    c_w = spec_w - 72
    glyph_hi = Image.new('L', (c_w * SCALE, 220 * SCALE), 0)
    gd = ImageDraw.Draw(glyph_hi)

    # Large 22pt
    gd.text((0, 0), "1. GROSS (22pt Hero-Ziffern & Einheiten):", font=f_m_hi, fill=160)
    gd.text((0, 18 * SCALE), "0 1 2 3 4 5 6 7 8 9   : . % ° C G M + -   47.5°C   23:14", font=f_l_hi, fill=255)

    # Medium 14pt
    gd.text((0, 58 * SCALE), "2. MITTEL (14pt Badges, Werte & Taktraten):", font=f_m_hi, fill=160)
    gd.text((0, 76 * SCALE), "0 1 2 3 4 5 6 7 8 9   A B C D E F G H I K M N P R S T U V W X Y Z   : . % ° C M G B Hz [ ]", font=f_m_hi, fill=255)
    gd.text((0, 94 * SCALE), "CPU 47.5°C   3618 RPM   1250 MHz   RAID 1 [UU]   NVMe 480G / 1.0TB (42%)", font=f_m_hi, fill=255)

    # Small 10pt
    gd.text((0, 128 * SCALE), "3. KLEIN (10pt Fließtext, IP, Uptime & Datum - Vollständiges Alphabet):", font=f_m_hi, fill=160)
    gd.text((0, 148 * SCALE), "Groß: A B C D E F G H I J K L M N O P Q R S T U V W X Y Z  |  Ziffern: 0 1 2 3 4 5 6 7 8 9", font=f_s_hi, fill=255)
    gd.text((0, 164 * SCALE), "Klein: a b c d e f g h i j k l m n o p q r s t u v w x y z  |  Umlaute: Ä Ö Ü ä ö ü ß", font=f_s_hi, fill=255)
    gd.text((0, 180 * SCALE), "Satzzeichen: : . , ; / - _ + = % [ ] ( ) ! ? # @ °C  ↑ 14.2 MB/s  ↓ 52.8 MB/s", font=f_s_hi, fill=255)
    gd.text((0, 196 * SCALE), "Beispiel: Dienstag, 27. Oktober 2026 | IP: 192.168.0.143 | Uptime: 54d 13h 22m", font=f_s_hi, fill=255)

    # Downsample cleanly to target resolution
    glyph_down = glyph_hi.resize((c_w, 220), Image.Resampling.LANCZOS)
    glyph_1bit = glyph_down.point(lambda p: 255 if p >= 100 else 0, mode='1')

    glyph_rgb = Image.new('RGB', (c_w, 220), (22, 27, 34))
    gr_draw = ImageDraw.Draw(glyph_rgb)
    for y in range(220):
        for x in range(c_w):
            if glyph_1bit.getpixel((x, y)):
                gr_draw.point((x, y), fill=(0, 229, 255))
    sheet.paste(glyph_rgb, (36, 130))

    # -------------------------------------------------------------------------
    # 2. Live OLED Screens (Natural, unadulterated rendering)
    # -------------------------------------------------------------------------
    draw.text((20, 380), "LIVE OLED-KACHELN (Natürliche Typografie auf 128×64 Auflösung):", fill=(240, 246, 252), font=sys_sec)

    def make_natural_mockup(screen_type):
        canvas_hi = Image.new('L', (128 * SCALE, 64 * SCALE), 0)
        d = ImageDraw.Draw(canvas_hi)

        if screen_type == "cpu":
            d.rectangle([1 * SCALE, 1 * SCALE, 26 * SCALE, 10 * SCALE], fill=255)
            d.text((3 * SCALE, -1 * SCALE), "CPU", font=f_s_hi, fill=0)
            d.text((32 * SCALE, -1 * SCALE), "3.0 GHz", font=f_s_hi, fill=255)
            d.line([1 * SCALE, 11 * SCALE, 126 * SCALE, 11 * SCALE], fill=255, width=SCALE)
            d.text((2 * SCALE, 13 * SCALE), "47.5°C", font=f_l_hi, fill=255)
            d.text((78 * SCALE, 15 * SCALE), "3618", font=f_m_hi, fill=255)
            d.text((78 * SCALE, 30 * SCALE), "RPM", font=f_s_hi, fill=255)
            d.line([1 * SCALE, 45 * SCALE, 126 * SCALE, 45 * SCALE], fill=255, width=SCALE)
            for c in range(4):
                bx = 2 + c * 31
                d.text((bx * SCALE, 48 * SCALE), f"C{c+1}", font=f_s_hi, fill=255)
                d.rectangle([(bx + 14) * SCALE, 51 * SCALE, (bx + 27) * SCALE, 58 * SCALE], outline=255, width=SCALE)
                d.rectangle([(bx + 15) * SCALE, 52 * SCALE, (bx + 22) * SCALE, 57 * SCALE], fill=255)

        elif screen_type == "clock":
            d.rectangle([1 * SCALE, 1 * SCALE, 126 * SCALE, 10 * SCALE], fill=255)
            d.text((22 * SCALE, -1 * SCALE), "ARGON ONE V5", font=f_s_hi, fill=0)
            d.text((25 * SCALE, 13 * SCALE), "23:14", font=f_l_hi, fill=255)
            d.line([1 * SCALE, 44 * SCALE, 126 * SCALE, 44 * SCALE], fill=255, width=SCALE)
            d.text((10 * SCALE, 46 * SCALE), "Dienstag, 27. Okt 2026", font=f_s_hi, fill=255)
            d.text((18 * SCALE, 54 * SCALE), "Uptime: 54d 13h 22m", font=f_s_hi, fill=255)

        elif screen_type == "storage":
            d.rectangle([1 * SCALE, 1 * SCALE, 38 * SCALE, 10 * SCALE], fill=255)
            d.text((3 * SCALE, -1 * SCALE), "RAID 1", font=f_s_hi, fill=0)
            d.text((43 * SCALE, -1 * SCALE), "[UU] HEALTHY", font=f_s_hi, fill=255)
            d.text((100 * SCALE, -1 * SCALE), "33°C", font=f_s_hi, fill=255)
            d.line([1 * SCALE, 11 * SCALE, 126 * SCALE, 11 * SCALE], fill=255, width=SCALE)
            d.text((2 * SCALE, 14 * SCALE), "42%", font=f_m_hi, fill=255)
            d.text((36 * SCALE, 16 * SCALE), "480G / 1.0TB NVMe", font=f_s_hi, fill=255)
            d.rectangle([2 * SCALE, 30 * SCALE, 125 * SCALE, 37 * SCALE], outline=255, width=SCALE)
            d.rectangle([3 * SCALE, 31 * SCALE, 55 * SCALE, 36 * SCALE], fill=255)
            d.line([1 * SCALE, 41 * SCALE, 126 * SCALE, 41 * SCALE], fill=255, width=SCALE)
            d.text((2 * SCALE, 44 * SCALE), "RAM", font=f_m_hi, fill=255)
            d.text((36 * SCALE, 46 * SCALE), "3.8G / 8.0G (48%)", font=f_s_hi, fill=255)
            d.rectangle([36 * SCALE, 56 * SCALE, 125 * SCALE, 61 * SCALE], outline=255, width=SCALE)
            d.rectangle([37 * SCALE, 57 * SCALE, 79 * SCALE, 60 * SCALE], fill=255)

        # Downscale to 128x64
        oled_down = canvas_hi.resize((128, 64), Image.Resampling.LANCZOS)
        oled_1bit = oled_down.point(lambda p: 255 if p >= 100 else 0, mode='1')

        scaled = Image.new('RGB', (256, 128), (8, 12, 18))
        sdraw = ImageDraw.Draw(scaled)
        for y in range(64):
            for x in range(128):
                if oled_1bit.getpixel((x, y)):
                    sdraw.rectangle([x*2, y*2, x*2+1, y*2+1], fill=(0, 229, 255))
        return scaled

    m_cpu = make_natural_mockup("cpu")
    m_clock = make_natural_mockup("clock")
    m_storage = make_natural_mockup("storage")

    x1, x2, x3 = 20, 345, 670
    y_m = 410

    sheet.paste(m_cpu, (x1, y_m))
    sheet.paste(m_clock, (x2, y_m))
    sheet.paste(m_storage, (x3, y_m))

    draw.rectangle([x1 - 2, y_m - 2, x1 + 258, y_m + 130], outline=(48, 54, 61), width=1)
    draw.rectangle([x2 - 2, y_m - 2, x2 + 258, y_m + 130], outline=(48, 54, 61), width=1)
    draw.rectangle([x3 - 2, y_m - 2, x3 + 258, y_m + 130], outline=(48, 54, 61), width=1)

    draw.text((x1, y_m + 136), "Kachel 1: CPU & Thermik", fill=(139, 148, 158), font=sys_bold)
    draw.text((x1, y_m + 152), "• Groß: 47.5°C Hero-Temperatur\n• Mittel: 3618 RPM Lüfter\n• Klein: Takt & C1-C4 Cores", fill=(110, 118, 129), font=sys_text)

    draw.text((x2, y_m + 136), "Kachel 2: Hero-Uhr & Datum", fill=(139, 148, 158), font=sys_bold)
    draw.text((x2, y_m + 152), "• Groß: 23:14 Digitaluhr\n• Klein: Dienstag, 27. Okt 2026\n• Klein: Uptime: 54d 13h 22m", fill=(110, 118, 129), font=sys_text)

    draw.text((x3, y_m + 136), "Kachel 3: RAID & Speicher", fill=(139, 148, 158), font=sys_bold)
    draw.text((x3, y_m + 152), "• Mittel: 42% & RAM\n• Klein: [UU] HEALTHY, 33°C\n• Klein: 480G / 1.0TB NVMe", fill=(110, 118, 129), font=sys_text)

    # Comparison box
    draw.rectangle([20, 620, spec_w - 20, 810], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 630), "VORTEILE DER NATÜRLICHEN RENDERING-METHODE:", fill=(0, 229, 255), font=sys_sec)

    points = [
        ("0% künstliche Ecken:", "Keine erzwungenen 45°-Abschrägungen. Jeder Buchstabe und jede Ziffer behält ihre natürliche, harmonische Form."),
        ("Glatter, moderner Look:", "Die 2 und 3 haben natürliche Bögen, die 1 hat ihre klassische Fahne, die 0 ist ein sanftes Oval ohne diagonale Eckschnitte."),
        ("Kein Ausfransen oder Klumpen:", "Durch 4× Oversampling und Antialias-Binarisierung entstehen glatte Kanten ohne unruhige Stufen."),
        ("Perfektes Schriftbild im Kleintext:", "Fließtext wie 'Dienstag, 27. Oktober 2026' liest sich wie gedruckt, ohne dass Buchstaben eckig verbogen werden.")
    ]

    for idx, (head, desc) in enumerate(points):
        py = 658 + idx * 36
        draw.text((36, py), head, fill=(240, 246, 252), font=sys_bold)
        draw.text((36, py + 16), desc, fill=(139, 148, 158), font=sys_text)

    out_paths = [
        "/home/playx/.gemini/antigravity/brain/76f12623-5a2e-4fdd-826a-0df57c2ceb4e/argon_natural_font_specimen.png",
        "/home/playx/Downloads/argon-v5-oled/screenshots/font_specimen_natural.png"
    ]
    for p in out_paths:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        sheet.save(p, "PNG")
        print(f"Specimen saved to {p}")

if __name__ == "__main__":
    render_natural_specimen()
