#!/usr/bin/env python3
"""
Generates C and Python font tables for Terminus Bold (6x12, 8x16, 11x22).
Outputs:
- src/font.c and src/font.h (for the C OLED daemon)
- Updates oled_engine.py (for the Python Card Builder)
"""

import os
from PIL import Image, ImageDraw, ImageFont

FONT_FILE = 'tools/fonts/terminus-bold.otb'

# Characters definition
ASCII_CHARS = [chr(c) for c in range(32, 127)] # 32 (' ') to 126 ('~') -> 95 chars
# Extended symbols:
# 95: Ä, 96: Ö, 97: Ü, 98: ä, 99: ö, 100: ü, 101: ß, 102: °, 103: ↑, 104: ↓
EXT_CHARS = ['Ä', 'Ö', 'Ü', 'ä', 'ö', 'ü', 'ß', '°', '\u2191', '\u2193']
ALL_CHARS_SMALL_MED = ASCII_CHARS + EXT_CHARS

LARGE_CHARS = [
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    ':', '.', '%', '°', 'C', 'G', 'M', 'K', 'B', '+', '-', ' '
]

def render_char(font, ch, w, h):
    im = Image.new('1', (w, h), 0)
    d = ImageDraw.Draw(im)
    d.text((0, 0), ch, font=font, fill=1)
    return im

def build_tables():
    f12 = ImageFont.truetype(FONT_FILE, 12)
    f16 = ImageFont.truetype(FONT_FILE, 16)
    f22 = ImageFont.truetype(FONT_FILE, 22)

    # 1. Terminus 6x12 (12 rows of 6 bits, MSB-left aligned in uint8_t)
    table_6x12 = []
    for ch in ALL_CHARS_SMALL_MED:
        im = render_char(f12, ch, 6, 12)
        rows = []
        for y in range(12):
            val = sum((im.getpixel((x, y)) << (7 - x)) for x in range(6))
            rows.append(val)
        table_6x12.append(rows)

    # 2. Terminus 8x16 (16 rows of 8 bits, 1 byte per row)
    table_8x16 = []
    for ch in ALL_CHARS_SMALL_MED:
        im = render_char(f16, ch, 8, 16)
        rows = []
        for y in range(16):
            val = sum((im.getpixel((x, y)) << (7 - x)) for x in range(8))
            rows.append(val)
        table_8x16.append(rows)

    # 3. Terminus 11x22 (22 rows of 11 bits, stored as uint16_t MSB-left aligned)
    table_11x22 = []
    for ch in LARGE_CHARS:
        im = render_char(f22, ch, 11, 22)
        rows = []
        for y in range(22):
            val = sum((im.getpixel((x, y)) << (15 - x)) for x in range(11))
            rows.append(val)
        table_11x22.append(rows)

    return table_6x12, table_8x16, table_11x22

if __name__ == '__main__':
    t6, t8, t11 = build_tables()
    print(f"Generated 6x12: {len(t6)} glyphs")
    print(f"Generated 8x16: {len(t8)} glyphs")
    print(f"Generated 11x22: {len(t11)} glyphs")
