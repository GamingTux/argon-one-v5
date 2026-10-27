#!/usr/bin/env python3
"""
Updates oled_engine.py with Terminus Bold bitmap tables and UTF-8 support.
"""

from PIL import Image, ImageDraw, ImageFont

FONT_FILE = 'tools/fonts/terminus-bold.otb'

ASCII_CHARS = [chr(c) for c in range(32, 127)] # 32 (' ') to 126 ('~') -> 95 chars
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

def generate_python_engine():
    f12 = ImageFont.truetype(FONT_FILE, 12)
    f16 = ImageFont.truetype(FONT_FILE, 16)
    f22 = ImageFont.truetype(FONT_FILE, 22)

    # 1. 6x12
    t6 = []
    for ch in ALL_CHARS_SMALL_MED:
        im = render_char(f12, ch, 6, 12)
        rows = [sum((im.getpixel((x, y)) << (7 - x)) for x in range(6)) for y in range(12)]
        t6.append(rows)

    # 2. 8x16
    t8 = []
    for ch in ALL_CHARS_SMALL_MED:
        im = render_char(f16, ch, 8, 16)
        rows = [sum((im.getpixel((x, y)) << (7 - x)) for x in range(8)) for y in range(16)]
        t8.append(rows)

    # 3. 11x22 (uint16_t rows)
    t11 = []
    for ch in LARGE_CHARS:
        im = render_char(f22, ch, 11, 22)
        rows = [sum((im.getpixel((x, y)) << (15 - x)) for x in range(11)) for y in range(22)]
        t11.append(rows)

    with open('oled_engine.py', 'r') as f:
        content = f.read()

    # Extract CUSTOM_ICONS and ICONS_16X16 and other parts from oled_engine.py
    # We want to replace FONT_5X7, FONT_6X12, FONT_12X20 with Terminus tables
    
    font_defs = f"""# ============================================================================
# Terminus Bold Handcrafted 1-Bit Pixel Fonts (Author: Dimitar Zhekov)
# 105 Glyphs: ASCII 32..126 + Ä, Ö, Ü, ä, ö, ü, ß, °, ↑, ↓
# ============================================================================

TERMINUS_CHARS = {''.join(ALL_CHARS_SMALL_MED)!r}
TERMINUS_CHAR_MAP = {{ch: idx for idx, ch in enumerate(TERMINUS_CHARS)}}
TERMINUS_CHAR_MAP['^'] = TERMINUS_CHAR_MAP['°']

TERMINUS_LARGE_CHARS = {''.join(LARGE_CHARS)!r}
TERMINUS_LARGE_MAP = {{ch: idx for idx, ch in enumerate(TERMINUS_LARGE_CHARS)}}
TERMINUS_LARGE_MAP['^'] = TERMINUS_LARGE_MAP['°']

FONT_TERMINUS_6X12 = {t6!r}
FONT_TERMINUS_8X16 = {t8!r}
FONT_TERMINUS_11X22 = {t11!r}

# Legacy aliases
FONT_5X7 = FONT_TERMINUS_6X12
FONT_6X12 = FONT_TERMINUS_8X16
FONT_12X20 = FONT_TERMINUS_11X22
"""

    # We will replace from start of FONT_5X7 to end of FONT_12X20
    start_pos = content.find("FONT_5X7 = ")
    end_marker = "ICONS_16X16 = "
    end_pos = content.find(end_marker)

    new_content = content[:start_pos] + font_defs + "\n" + content[end_pos:]

    # Now replace draw_char_small, draw_string_small, draw_char_med, draw_string_med, draw_char_large, draw_string_large
    old_methods_start = new_content.find("    def draw_char_small(self, x, y, char, color=1):")
    old_methods_end = new_content.find("    def draw_icon(self, x, y, name, color=1, w=None, h=None):")

    new_methods = """    def draw_char_small(self, x, y, char, color=1):
        idx = TERMINUS_CHAR_MAP.get(char, TERMINUS_CHAR_MAP.get('?'))
        if idx is not None and 0 <= idx < len(FONT_TERMINUS_6X12):
            glyph = FONT_TERMINUS_6X12[idx]
            for row_idx, row_byte in enumerate(glyph):
                for bit in range(6):
                    if (row_byte >> (7 - bit)) & 1:
                        self.pixel(x + bit, y + row_idx, color)
        return 6

    def draw_string_small(self, x, y, text, color=1):
        cx = x
        for ch in text:
            self.draw_char_small(cx, y, ch, color)
            cx += 6
        return cx - x

    def draw_char_med(self, x, y, char, color=1):
        idx = TERMINUS_CHAR_MAP.get(char, TERMINUS_CHAR_MAP.get('?'))
        if idx is not None and 0 <= idx < len(FONT_TERMINUS_8X16):
            glyph = FONT_TERMINUS_8X16[idx]
            for row_idx, row_byte in enumerate(glyph):
                for bit in range(8):
                    if (row_byte >> (7 - bit)) & 1:
                        self.pixel(x + bit, y + row_idx, color)
        return 8

    def draw_string_med(self, x, y, text, color=1):
        cx = x
        for ch in text:
            self.draw_char_med(cx, y, ch, color)
            cx += 8
        return cx - x

    def draw_char_large(self, x, y, char, color=1):
        idx = TERMINUS_LARGE_MAP.get(char)
        if idx is not None and 0 <= idx < len(FONT_TERMINUS_11X22):
            glyph = FONT_TERMINUS_11X22[idx]
            for row_idx, row_word in enumerate(glyph):
                for bit in range(11):
                    if (row_word >> (15 - bit)) & 1:
                        self.pixel(x + bit, y + row_idx, color)
        if char in (':', '.'): return 6
        if char in ('°', '^'): return 8
        return 11

    def draw_string_large(self, x, y, text, color=1):
        cx = x
        for ch in text:
            adv = self.draw_char_large(cx, y, ch, color)
            cx += adv
        return cx - x

"""
    new_content = new_content[:old_methods_start] + new_methods + new_content[old_methods_end:]

    with open('oled_engine.py', 'w') as f:
        f.write(new_content)
    print("Successfully updated oled_engine.py with Terminus Bold!")

if __name__ == '__main__':
    generate_python_engine()
