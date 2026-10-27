#!/usr/bin/env python3
"""
Argon ONE V5 - Pure 1-Bit Pixel-Perfect Fonts Specimen (Terminus & Spleen)
Compares genuine handcrafted 1-bit bitmap fonts that eliminate all vector rasterization artifacts.
"""

import os
from PIL import Image, ImageDraw, ImageFont

def render_pixel_specimen():
    term_bold = 'tools/fonts/terminus-bold.otb'
    spleen_6x12 = 'tools/fonts/spleen-6x12.otf'
    spleen_8x16 = 'tools/fonts/spleen-8x16.otf'
    spleen_12x24 = 'tools/fonts/spleen-12x24.otf'

    spec_w, spec_h = 1000, 1020
    sheet = Image.new('RGB', (spec_w, spec_h), (13, 17, 23))
    draw = ImageDraw.Draw(sheet)

    sys_title = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf', 20)
    sys_sub = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf', 12)
    sys_sec = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf', 14)
    sys_bold = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf', 12)
    sys_text = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf', 11)

    # 1. Header Card
    draw.rectangle([20, 20, spec_w - 20, 85], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 30), "ARGON ONE V5 — ECHTE 1-BIT PIXEL-PERFEKTE SCHRIFTEN", fill=(0, 229, 255), font=sys_title)
    draw.text((36, 58), "Vergleich: Terminus Bold (Industrie-Goldstandard) vs. Spleen (Moderner Pixel-Look) — 0% Interpolation", fill=(139, 148, 158), font=sys_sub)

    # -------------------------------------------------------------------------
    # Helper to render 128x64 OLED canvas and upscale 2x
    # -------------------------------------------------------------------------
    def build_oled_mockup(font_type, screen):
        im = Image.new('1', (128, 64), 0)
        d = ImageDraw.Draw(im)

        if font_type == "terminus":
            f_s = ImageFont.truetype(term_bold, 12)
            f_m = ImageFont.truetype(term_bold, 16)
            f_l = ImageFont.truetype(term_bold, 22)
        else: # spleen
            f_s = ImageFont.truetype(spleen_6x12, 12)
            f_m = ImageFont.truetype(spleen_8x16, 16)
            f_l = ImageFont.truetype(spleen_12x24, 24)

        if screen == "cpu":
            d.rectangle([1, 1, 26, 11], fill=1)
            d.text((4, 0), "CPU", font=f_s, fill=0)
            d.text((32, 0), "3.0 GHz", font=f_s, fill=1)
            d.line([1, 12, 126, 12], fill=1)

            d.text((2, 16), "47.5°C", font=f_l, fill=1)
            d.text((82, 16), "3618", font=f_m, fill=1)
            d.text((82, 33), "RPM", font=f_s, fill=1)

            d.line([1, 46, 126, 46], fill=1)
            for c in range(4):
                bx = 2 + c * 31
                d.text((bx, 48), f"C{c+1}", font=f_s, fill=1)
                d.rectangle([bx + 14, 50, bx + 27, 58], outline=1)
                d.rectangle([bx + 15, 51, bx + 22, 57], fill=1)

        elif screen == "clock":
            d.rectangle([1, 1, 126, 11], fill=1)
            d.text((25, 0), "ARGON ONE V5", font=f_s, fill=0)
            d.line([1, 12, 126, 12], fill=1)

            d.text((28, 16), "23:14", font=f_l, fill=1)

            d.line([1, 44, 126, 44], fill=1)
            d.text((2, 46), "Di, 27. Okt 2026", font=f_s, fill=1)
            d.text((2, 54), "Uptime: 54d 13h 22m", font=f_s, fill=1)

        elif screen == "storage":
            d.rectangle([1, 1, 38, 11], fill=1)
            d.text((3, 0), "RAID 1", font=f_s, fill=0)
            d.text((43, 0), "[UU] OK", font=f_s, fill=1)
            d.text((95, 0), "33°C", font=f_s, fill=1)
            d.line([1, 12, 126, 12], fill=1)

            d.text((2, 16), "42%", font=f_m, fill=1)
            d.text((38, 18), "480G/1.0TB NVMe", font=f_s, fill=1)
            d.rectangle([2, 32, 125, 38], outline=1)
            d.rectangle([3, 33, 54, 37], fill=1)

            d.line([1, 42, 126, 42], fill=1)
            d.text((2, 46), "RAM", font=f_m, fill=1)
            d.text((38, 48), "3.8G/8.0G (48%)", font=f_s_hi if 'f_s_hi' in locals() else f_s, fill=1)
            d.rectangle([38, 57, 125, 62], outline=1)
            d.rectangle([39, 58, 80, 61], fill=1)

        # Upscale 2x for crisp viewing
        scaled = Image.new('RGB', (256, 128), (8, 12, 18))
        sdraw = ImageDraw.Draw(scaled)
        for y in range(64):
            for x in range(128):
                if im.getpixel((x, y)):
                    sdraw.rectangle([x*2, y*2, x*2+1, y*2+1], fill=(0, 229, 255))
        return scaled

    # -------------------------------------------------------------------------
    # SECTION 1: TERMINUS BOLD (Row of 3 screens + character set)
    # -------------------------------------------------------------------------
    y_sec1 = 100
    draw.rectangle([20, y_sec1, spec_w - 20, y_sec1 + 380], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, y_sec1 + 10), "OPTION A: TERMINUS BOLD (DER INDUSTRIE-STANDARD)", fill=(0, 229, 255), font=sys_sec)
    draw.text((36, y_sec1 + 28), "Handgezeichnete Bitmap-Schrift von Dimitar Zhekov | 6×12 (Klein), 8×16 (Mittel), 11×22 (Groß)", fill=(139, 148, 158), font=sys_sub)

    # Glyphs sample
    f_t_s = ImageFont.truetype(term_bold, 12)
    f_t_m = ImageFont.truetype(term_bold, 16)
    f_t_l = ImageFont.truetype(term_bold, 22)

    im_t_glyphs = Image.new('1', (spec_w - 72, 80), 0)
    dtg = ImageDraw.Draw(im_t_glyphs)
    dtg.text((0, 0), "Groß 11×22:   0 1 2 3 4 5 6 7 8 9  : . % ° C G M + -   47.5°C   23:14", font=f_t_l, fill=1)
    dtg.text((0, 28), "Mittel 8×16:  CPU 47.5°C   3618 RPM   1250 MHz   RAID 1 [UU]   NVMe 480G / 1.0TB (42%)", font=f_t_m, fill=1)
    dtg.text((0, 48), "Klein 6×12:   A-Z a-z 0-9 Ä Ö Ü ä ö ü ß : . , ; / - _ + = % [ ] ( ) ! ? # @ °C ↑ 14.2 MB/s ↓ 52.8 MB/s", font=f_t_s, fill=1)
    dtg.text((0, 62), "Fließtext:    Dienstag, 27. Oktober 2026 | IP: 192.168.0.143 | Uptime: 54d 13h 22m", font=f_t_s, fill=1)

    # Paste Terminus Glyphs
    rgb_tg = Image.new('RGB', (spec_w - 72, 80), (22, 27, 34))
    rdtg = ImageDraw.Draw(rgb_tg)
    for y in range(80):
        for x in range(spec_w - 72):
            if im_t_glyphs.getpixel((x, y)):
                rdtg.point((x, y), fill=(0, 229, 255))
    sheet.paste(rgb_tg, (36, y_sec1 + 52))

    # Terminus Live Mockups
    t_cpu = build_oled_mockup("terminus", "cpu")
    t_clock = build_oled_mockup("terminus", "clock")
    t_store = build_oled_mockup("terminus", "storage")

    x1, x2, x3 = 36, 360, 684
    ym1 = y_sec1 + 145
    sheet.paste(t_cpu, (x1, ym1))
    sheet.paste(t_clock, (x2, ym1))
    sheet.paste(t_store, (x3, ym1))

    draw.rectangle([x1 - 2, ym1 - 2, x1 + 258, ym1 + 130], outline=(48, 54, 61), width=1)
    draw.rectangle([x2 - 2, ym1 - 2, x2 + 258, ym1 + 130], outline=(48, 54, 61), width=1)
    draw.rectangle([x3 - 2, ym1 - 2, x3 + 258, ym1 + 130], outline=(48, 54, 61), width=1)

    draw.text((x1, ym1 + 135), "Terminus: CPU & Thermik (128×64)", fill=(139, 148, 158), font=sys_bold)
    draw.text((x2, ym1 + 135), "Terminus: Hero-Uhr & Datum (128×64)", fill=(139, 148, 158), font=sys_bold)
    draw.text((x3, ym1 + 135), "Terminus: RAID & Speicher (128×64)", fill=(139, 148, 158), font=sys_bold)

    draw.text((36, ym1 + 160), "Charakteristik Terminus: Absolut makellose Lesbarkeit. Perfekte Proportionen, serifenlose Ziffern, runde Bögen ohne Treppchen.", fill=(88, 166, 255), font=sys_text)

    # -------------------------------------------------------------------------
    # SECTION 2: SPLEEN (Row of 3 screens + character set)
    # -------------------------------------------------------------------------
    y_sec2 = 500
    draw.rectangle([20, y_sec2, spec_w - 20, y_sec2 + 380], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, y_sec2 + 10), "OPTION B: SPLEEN (DER MODERNE RETRO-PIXEL-LOOK)", fill=(0, 229, 255), font=sys_sec)
    draw.text((36, y_sec2 + 28), "Entwickelt von Frederic Cambus speziell für kleine Displays & OLEDs | 6×12, 8×16, 12×24", fill=(139, 148, 158), font=sys_sub)

    # Spleen Glyphs sample
    f_sp_s = ImageFont.truetype(spleen_6x12, 12)
    f_sp_m = ImageFont.truetype(spleen_8x16, 16)
    f_sp_l = ImageFont.truetype(spleen_12x24, 24)

    im_sp_glyphs = Image.new('1', (spec_w - 72, 85), 0)
    dspg = ImageDraw.Draw(im_sp_glyphs)
    dspg.text((0, 0), "Groß 12×24:   0 1 2 3 4 5 6 7 8 9  : . % ° C G M + -   47.5°C   23:14", font=f_sp_l, fill=1)
    dspg.text((0, 30), "Mittel 8×16:  CPU 47.5°C   3618 RPM   1250 MHz   RAID 1 [UU]   NVMe 480G / 1.0TB (42%)", font=f_sp_m, fill=1)
    dspg.text((0, 50), "Klein 6×12:   A-Z a-z 0-9 Ä Ö Ü ä ö ü ß : . , ; / - _ + = % [ ] ( ) ! ? # @ °C ↑ 14.2 MB/s ↓ 52.8 MB/s", font=f_sp_s, fill=1)
    dspg.text((0, 65), "Fließtext:    Dienstag, 27. Oktober 2026 | IP: 192.168.0.143 | Uptime: 54d 13h 22m", font=f_sp_s, fill=1)

    # Paste Spleen Glyphs
    rgb_spg = Image.new('RGB', (spec_w - 72, 85), (22, 27, 34))
    rdspg = ImageDraw.Draw(rgb_spg)
    for y in range(85):
        for x in range(spec_w - 72):
            if im_sp_glyphs.getpixel((x, y)):
                rdspg.point((x, y), fill=(0, 229, 255))
    sheet.paste(rgb_spg, (36, y_sec2 + 52))

    # Spleen Live Mockups
    sp_cpu = build_oled_mockup("spleen", "cpu")
    sp_clock = build_oled_mockup("spleen", "clock")
    sp_store = build_oled_mockup("spleen", "storage")

    ym2 = y_sec2 + 150
    sheet.paste(sp_cpu, (x1, ym2))
    sheet.paste(sp_clock, (x2, ym2))
    sheet.paste(sp_store, (x3, ym2))

    draw.rectangle([x1 - 2, ym2 - 2, x1 + 258, ym2 + 130], outline=(48, 54, 61), width=1)
    draw.rectangle([x2 - 2, ym2 - 2, x2 + 258, ym2 + 130], outline=(48, 54, 61), width=1)
    draw.rectangle([x3 - 2, ym2 - 2, x3 + 258, ym2 + 130], outline=(48, 54, 61), width=1)

    draw.text((x1, ym2 + 135), "Spleen: CPU & Thermik (128×64)", fill=(139, 148, 158), font=sys_bold)
    draw.text((x2, ym2 + 135), "Spleen: Hero-Uhr & Datum (128×64)", fill=(139, 148, 158), font=sys_bold)
    draw.text((x3, ym2 + 135), "Spleen: RAID & Speicher (128×64)", fill=(139, 148, 158), font=sys_bold)

    draw.text((36, ym2 + 160), "Charakteristik Spleen: Markanter, leicht kantiger Retro-/Cyberpunk-Charakter mit perfekter 1-Bit-Präzision.", fill=(88, 166, 255), font=sys_text)

    # -------------------------------------------------------------------------
    # SUMMARY FOOTER
    # -------------------------------------------------------------------------
    draw.rectangle([20, 900, spec_w - 20, 1000], fill=(22, 27, 34), outline=(48, 54, 61), width=1)
    draw.text((36, 910), "WARUM DIESE SCHRIFTEN JEDER VEKTORSCHRIFT AUF DEM OLED ÜBERLEGEN SIND:", fill=(0, 229, 255), font=sys_sec)
    draw.text((36, 930), "• Kein Vektor-Rundungsfehler: Jeder einzelne Pixel wurde von Hand im Gitter gezeichnet (keine Bézier-Interpolation).", fill=(240, 246, 252), font=sys_text)
    draw.text((36, 948), "• 100% einheitliche Strichbreiten: Keine zufälligen 1px/2px Breitenunterschiede oder verwaschene Rundungen.", fill=(240, 246, 252), font=sys_text)
    draw.text((36, 966), "• Vollständige deutsche Zeichensätze: Ä, Ö, Ü, ß, Sonderzeichen und Symbole sind nativ enthalten.", fill=(240, 246, 252), font=sys_text)

    out_paths = [
        "/home/playx/.gemini/antigravity/brain/76f12623-5a2e-4fdd-826a-0df57c2ceb4e/argon_pixel_perfect_specimen.png",
        "/home/playx/Downloads/argon-v5-oled/screenshots/font_specimen_pixel_perfect.png"
    ]
    for p in out_paths:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        sheet.save(p, "PNG")
        print(f"Specimen saved to {p}")

if __name__ == "__main__":
    render_pixel_specimen()
