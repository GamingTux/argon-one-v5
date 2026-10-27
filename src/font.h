#ifndef FONT_H
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
