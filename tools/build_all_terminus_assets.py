#!/usr/bin/env python3
"""
Generates C and Python tables for Terminus Bold and updates font files.
"""

import os
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

def build_tables():
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

    return t6, t8, t11

def generate_c_font_c(t6, t8, t11):
    c_code = """#include "font.h"

// ============================================================================
// Terminus Bold Handcrafted 1-Bit Pixel Fonts
// Author: Dimitar Zhekov
// Optimized for Argon ONE V5 OLED Display (128x64)
// ============================================================================

// ----------------------------------------------------------------------------
// 1. Terminus Bold 6x12 (Small / Micro) - 105 Glyphs
// Stored as 12 rows of 6 bits (MSB-left aligned in uint8_t)
// ----------------------------------------------------------------------------
const uint8_t font_terminus_6x12[105][12] = {
"""
    for idx, rows in enumerate(t6):
        ch = ALL_CHARS_SMALL_MED[idx]
        ch_desc = ch if ord(ch) >= 32 else f"\\x{ord(ch):02x}"
        row_hex = ", ".join(f"0x{r:02X}" for r in rows)
        c_code += f"    {{ {row_hex} }}, // {idx:3d} '{ch_desc}'\n"
    c_code += "};\n\n"

    # 8x16
    c_code += """// ----------------------------------------------------------------------------
// 2. Terminus Bold 8x16 (Medium / Badges / Headers) - 105 Glyphs
// Stored as 16 rows of 8 bits (1 byte per row)
// ----------------------------------------------------------------------------
const uint8_t font_terminus_8x16[105][16] = {
"""
    for idx, rows in enumerate(t8):
        ch = ALL_CHARS_SMALL_MED[idx]
        ch_desc = ch if ord(ch) >= 32 else f"\\x{ord(ch):02x}"
        row_hex = ", ".join(f"0x{r:02X}" for r in rows)
        c_code += f"    {{ {row_hex} }}, // {idx:3d} '{ch_desc}'\n"
    c_code += "};\n\n"

    # 11x22
    c_code += """// ----------------------------------------------------------------------------
// 3. Terminus Bold 11x22 (Large / Hero Digits & Clock) - 22 Glyphs
// Stored as 22 rows of uint16_t (11 bits MSB-aligned, 0x8000 >> col)
// ----------------------------------------------------------------------------
const uint16_t font_terminus_11x22[22][22] = {
"""
    for idx, rows in enumerate(t11):
        ch = LARGE_CHARS[idx]
        row_hex = ", ".join(f"0x{r:04X}" for r in rows)
        c_code += f"    // {idx:2d} '{ch}'\n    {{\n"
        for chunk_idx in range(0, 22, 11):
            chunk = rows[chunk_idx:chunk_idx+11]
            c_code += "        " + ", ".join(f"0x{r:04X}" for r in chunk) + ",\n"
        c_code += "    },\n"
    c_code += "};\n\n"

    # UTF-8 and helper routines
    c_code += """// ----------------------------------------------------------------------------
// UTF-8 Lookup Helper for Terminus 6x12 & 8x16
// Maps UTF-8 characters to table index 0..104
// ----------------------------------------------------------------------------
int font_terminus_lookup_utf8(const char *s, int *bytes_consumed) {
    if (!s || !*s) {
        if (bytes_consumed) *bytes_consumed = 0;
        return -1;
    }
    unsigned char c1 = (unsigned char)s[0];
    if (c1 == '^') {
        if (bytes_consumed) *bytes_consumed = 1;
        return 102; // '°'
    }
    if (c1 == 0xC2 && (unsigned char)s[1] == 0xB0) {
        if (bytes_consumed) *bytes_consumed = 2;
        return 102; // '°'
    }
    if (c1 == 0xC3) {
        unsigned char c2 = (unsigned char)s[1];
        if (bytes_consumed) *bytes_consumed = 2;
        if (c2 == 0x84) return 95;  // Ä
        if (c2 == 0x96) return 96;  // Ö
        if (c2 == 0x9C) return 97;  // Ü
        if (c2 == 0xA4) return 98;  // ä
        if (c2 == 0xB6) return 99;  // ö
        if (c2 == 0xBC) return 100; // ü
        if (c2 == 0x9F) return 101; // ß
    }
    if (c1 == 0xE2 && (unsigned char)s[1] == 0x86) {
        unsigned char c3 = (unsigned char)s[2];
        if (bytes_consumed) *bytes_consumed = 3;
        if (c3 == 0x91) return 103; // ↑
        if (c3 == 0x93) return 104; // ↓
    }
    if (c1 >= 32 && c1 <= 126) {
        if (bytes_consumed) *bytes_consumed = 1;
        return (int)(c1 - 32);
    }
    if (bytes_consumed) *bytes_consumed = 1;
    return (int)('?' - 32);
}

int font_terminus_large_index(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c == ':') return 10;
    if (c == '.') return 11;
    if (c == '%') return 12;
    if (c == '^') return 13; // °
    if (c == 'C' || c == 'c') return 14;
    if (c == 'G' || c == 'g') return 15;
    if (c == 'M' || c == 'm') return 16;
    if (c == 'K' || c == 'k') return 17;
    if (c == 'B' || c == 'b') return 18;
    if (c == '+') return 19;
    if (c == '-') return 20;
    if (c == ' ') return 21;
    return -1;
}

// ----------------------------------------------------------------------------
// Status Icons
// ----------------------------------------------------------------------------
const uint8_t icon_cpu[8]   = { 0x3C, 0x42, 0x99, 0xBD, 0xBD, 0x99, 0x42, 0x3C };
const uint8_t icon_temp[8]  = { 0x18, 0x24, 0x24, 0x24, 0x24, 0x5A, 0x7E, 0x3C };
const uint8_t icon_disk[8]  = { 0x7E, 0x81, 0xA5, 0x81, 0x81, 0xBD, 0x81, 0x7E };
const uint8_t icon_net[8]   = { 0x24, 0x66, 0xFF, 0xDB, 0x18, 0x3C, 0x3C, 0x18 };
const uint8_t icon_clock[8] = { 0x3C, 0x42, 0x85, 0x85, 0x87, 0x81, 0x42, 0x3C };
const uint8_t icon_ram[8]   = { 0xFE, 0x92, 0x92, 0xFE, 0x92, 0x92, 0xFE, 0x00 };
const uint8_t icon_eth[8]   = { 0x1E, 0x11, 0xFD, 0xF1, 0xF1, 0xFD, 0x11, 0x1E };
const uint8_t icon_wifi[8]  = { 0x02, 0x09, 0x25, 0x95, 0x95, 0x25, 0x09, 0x02 };
"""
    return c_code

def generate_c_font_h():
    return """#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include <stdbool.h>

// ----------------------------------------------------------------------------
// Terminus Bold Handcrafted 1-Bit Pixel Fonts
// ----------------------------------------------------------------------------

// Terminus Bold 6x12 (Small / Micro) - 105 Glyphs (ASCII + Ä Ö Ü ä ö ü ß ° ↑ ↓)
extern const uint8_t font_terminus_6x12[105][12];

// Terminus Bold 8x16 (Medium / Badges / Headers) - 105 Glyphs
extern const uint8_t font_terminus_8x16[105][16];

// Terminus Bold 11x22 (Large / Hero Digits & Clock) - 22 Glyphs
extern const uint16_t font_terminus_11x22[22][22];

// UTF-8 & Large Index Helpers
int font_terminus_lookup_utf8(const char *s, int *bytes_consumed);
int font_terminus_large_index(char c);

// Icons
extern const uint8_t icon_cpu[8];
extern const uint8_t icon_temp[8];
extern const uint8_t icon_disk[8];
extern const uint8_t icon_net[8];
extern const uint8_t icon_clock[8];
extern const uint8_t icon_ram[8];
extern const uint8_t icon_eth[8];
extern const uint8_t icon_wifi[8];

#endif // FONT_H
"""

if __name__ == '__main__':
    t6, t8, t11 = build_tables()
    c_src = generate_c_font_c(t6, t8, t11)
    c_hdr = generate_c_font_h()

    with open('src/font.c', 'w') as f:
        f.write(c_src)
    with open('src/font.h', 'w') as f:
        f.write(c_hdr)
    print("Wrote src/font.c and src/font.h")
