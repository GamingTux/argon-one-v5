#!/usr/bin/env python3
"""
Argon ONE V5 - Liberation Font Family Specimen & OLED Mockups
Renders the complete character sets (Small 9pt, Medium 13pt, Large 22pt)
based on Liberation Sans Bold and Liberation Mono Bold.
"""

import os
from PIL import Image, ImageDraw, ImageFont

def render_liberation_specimen():
    font_sans = '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf'
    font_mono = '/usr/share/fonts/truetype/liberation/LiberationMono-Bold.ttf'

    spec_w, spec_h = 960, 840
    sheet = Image.new('RGB', (spec_w, spec_h), (13, 17, 23))
    draw = ImageDraw.Draw(sheet)

    try:
        sys_title = ImageFont.truetype(font_sans, 20)
        sys_sub = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf', 12)
        sys_sec = ImageFont.truetype(font_sans, 14)
        sys_bold = ImageFont.truetype(font_sans, 12)
        sys_text = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf', 11)
    except:
        sys_title = sys_sub = sys_sec = sys_bold = sys_text = ImageFont.load_default()

    # Header Card
    draw.rectangle([20, 20, spec_w - 20, 85], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 30), "ARGON ONE V5 — SCHRIFTSYSTEM: LIBERATION SANS BOLD", fill=(0, 229, 255), font=sys_title)
    draw.text((36, 58), "Klassisch-technischer DIN-Look | Klare Kanten, offene Formen, 100% einheitliche Familie in Klein, Mittel & Groß", fill=(139, 148, 158), font=sys_sub)

    f_s = ImageFont.truetype(font_sans, 10)
    f_m = ImageFont.truetype(font_sans, 14)
    f_l = ImageFont.truetype(font_sans, 22)

    # 1. Glyphs Overview Box
    draw.rectangle([20, 100, spec_w - 20, 360], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 110), "VOLLSTÄNDIGER ZEICHENSATZ (LIBERATION SANS BOLD)", fill=(0, 229, 255), font=sys_sec)

    c_w = spec_w - 72
    glyph_canvas = Image.new('L', (c_w, 220), 0)
    gd = ImageDraw.Draw(glyph_canvas)

    # Large 22pt
    gd.text((0, 0), "1. GROSS (22pt Hero-Ziffern & Einheiten):", font=f_m, fill=160)
    gd.text((0, 18), "0 1 2 3 4 5 6 7 8 9   : . % ° C G M + -   47.5°C   23:10", font=f_l, fill=255)

    # Medium 14pt
    gd.text((0, 58), "2. MITTEL (14pt Header, Badges & Taktraten):", font=f_m, fill=160)
    gd.text((0, 76), "0 1 2 3 4 5 6 7 8 9   A B C D E F G H I K M N P R S T U V W X Y Z   : . % ° C M G B Hz [ ]", font=f_m, fill=255)
    gd.text((0, 94), "CPU 47.5°C   3618 RPM   1250 MHz   RAID 1 [UU]   NVMe 480G / 1.0TB (42%)", font=f_m, fill=255)

    # Small 10pt
    gd.text((0, 128), "3. KLEIN (10pt Fließtext, IP, Uptime & Datum - Vollständiges deutsches Alphabet):", font=f_m, fill=160)
    gd.text((0, 148), "Groß: A B C D E F G H I J K L M N O P Q R S T U V W X Y Z  |  Ziffern: 0 1 2 3 4 5 6 7 8 9", font=f_s, fill=255)
    gd.text((0, 164), "Klein: a b c d e f g h i j k l m n o p q r s t u v w x y z  |  Umlaute: Ä Ö Ü ä ö ü ß", font=f_s, fill=255)
    gd.text((0, 180), "Satzzeichen: : . , ; / - _ + = % [ ] ( ) ! ? # @ °C  ↑ 14.2 MB/s  ↓ 52.8 MB/s", font=f_s, fill=255)
    gd.text((0, 196), "Beispiel: Dienstag, 27. Oktober 2026 | IP: 192.168.0.143 | Uptime: 54d 13h 22m", font=f_s, fill=255)

    # Binarize with optimal threshold
    glyph_1bit = glyph_canvas.point(lambda p: 255 if p >= 105 else 0, mode='1')

    glyph_rgb = Image.new('RGB', (c_w, 220), (22, 27, 34))
    gr_draw = ImageDraw.Draw(glyph_rgb)
    for y in range(220):
        for x in range(c_w):
            if glyph_1bit.getpixel((x, y)):
                gr_draw.point((x, y), fill=(0, 229, 255))
    sheet.paste(glyph_rgb, (36, 130))

    # -------------------------------------------------------------------------
    # 2. Live OLED Screens
    # -------------------------------------------------------------------------
    draw.text((20, 380), "LIVE OLED-KACHELN (Echte 128×64 Auflösung mit Liberation Sans Bold):", fill=(240, 246, 252), font=sys_sec)

    def make_mockup(screen_type):
        canvas = Image.new('L', (128, 64), 0)
        d = ImageDraw.Draw(canvas)

        if screen_type == "cpu":
            d.rectangle([1, 1, 26, 10], fill=255)
            d.text((3, -1), "CPU", font=f_s, fill=0)
            d.text((32, -1), "3.0 GHz", font=f_s, fill=255)
            d.line([1, 11, 126, 11], fill=255)
            d.text((2, 13), "47.5°C", font=f_l, fill=255)
            d.text((78, 15), "3618", font=f_m, fill=255)
            d.text((78, 30), "RPM", font=f_s, fill=255)
            d.line([1, 45, 126, 45], fill=255)
            for c in range(4):
                bx = 2 + c * 31
                d.text((bx, 48), f"C{c+1}", font=f_s, fill=255)
                d.rectangle([bx + 14, 51, bx + 27, 58], outline=255)
                d.rectangle([bx + 15, 52, bx + 22, 57], fill=255)

        elif screen_type == "clock":
            d.rectangle([1, 1, 126, 10], fill=255)
            d.text((22, -1), "ARGON ONE V5", font=f_s, fill=0)
            d.text((25, 13), "23:10", font=f_l, fill=255)
            d.line([1, 44, 126, 44], fill=255)
            d.text((10, 46), "Dienstag, 27. Okt 2026", font=f_s, fill=255)
            d.text((18, 54), "Uptime: 54d 13h 22m", font=f_s, fill=255)

        elif screen_type == "storage":
            d.rectangle([1, 1, 38, 10], fill=255)
            d.text((3, -1), "RAID 1", font=f_s, fill=0)
            d.text((43, -1), "[UU] HEALTHY", font=f_s, fill=255)
            d.text((100, -1), "33°C", font=f_s, fill=255)
            d.line([1, 11, 126, 11], fill=255)
            d.text((2, 14), "42%", font=f_m, fill=255)
            d.text((36, 16), "480G / 1.0TB NVMe", font=f_s, fill=255)
            d.rectangle([2, 30, 125, 37], outline=255)
            d.rectangle([3, 31, 55, 36], fill=255)
            d.line([1, 41, 126, 41], fill=255)
            d.text((2, 44), "RAM", font=f_m, fill=255)
            d.text((36, 46), "3.8G / 8.0G (48%)", font=f_s, fill=255)
            d.rectangle([36, 56, 125, 61], outline=255)
            d.rectangle([37, 57, 79, 60], fill=255)

        oled_1bit = canvas.point(lambda p: 255 if p >= 105 else 0, mode='1')
        scaled = Image.new('RGB', (256, 128), (8, 12, 18))
        sdraw = ImageDraw.Draw(scaled)
        for y in range(64):
            for x in range(128):
                if oled_1bit.getpixel((x, y)):
                    sdraw.rectangle([x*2, y*2, x*2+1, y*2+1], fill=(0, 229, 255))
        return scaled

    m_cpu = make_mockup("cpu")
    m_clock = make_mockup("clock")
    m_storage = make_mockup("storage")

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
    draw.text((x2, y_m + 152), "• Groß: 23:10 Digitaluhr\n• Klein: Dienstag, 27. Okt 2026\n• Klein: Uptime: 54d 13h 22m", fill=(110, 118, 129), font=sys_text)

    draw.text((x3, y_m + 136), "Kachel 3: RAID & Speicher", fill=(139, 148, 158), font=sys_bold)
    draw.text((x3, y_m + 152), "• Mittel: 42% & RAM\n• Klein: [UU] HEALTHY, 33°C\n• Klein: 480G / 1.0TB NVMe", fill=(110, 118, 129), font=sys_text)

    # Comparison box
    draw.rectangle([20, 620, spec_w - 20, 810], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 630), "WARUM LIBERATION SANS BOLD TECHNISCH ÜBERZEUGT:", fill=(0, 229, 255), font=sys_sec)

    points = [
        ("Nüchterner DIN-Charakter:", "Keine verspielten Schwünge wie bei DejaVu, sondern straffe, gerade Linien mit klaren rechten Winkeln."),
        ("100% Zeichenabdeckung:", "Vollständiges deutsches Alphabet (a-z, A-Z, ä ö ü ß), Ziffern und Symbole – keine fehlenden Zeichen oder Platzhalter."),
        ("Präzise Hero-Ziffern (22pt):", "Die großen Zahlen für Uhrzeit (23:10) und Temperatur (47.5°C) stehen felsenfest, ausgewogen und sauber."),
        ("Exzellente Lesbarkeit bei 10pt:", "Worte wie 'Dienstag, 27. Oktober 2026' sind kompakt, aber gestochen scharf und ohne Verschwimmen.")
    ]

    for idx, (head, desc) in enumerate(points):
        py = 658 + idx * 36
        draw.text((36, py), head, fill=(240, 246, 252), font=sys_bold)
        draw.text((36, py + 16), desc, fill=(139, 148, 158), font=sys_text)

    out_paths = [
        "/home/playx/.gemini/antigravity/brain/76f12623-5a2e-4fdd-826a-0df57c2ceb4e/argon_liberation_specimen.png",
        "/home/playx/Downloads/argon-v5-oled/screenshots/font_specimen_liberation.png"
    ]
    for p in out_paths:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        sheet.save(p, "PNG")
        print(f"Specimen saved to {p}")

if __name__ == "__main__":
    render_liberation_specimen()
