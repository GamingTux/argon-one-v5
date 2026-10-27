#ifndef FONT_H
#define FONT_H

#include <stdint.h>

// 5x7 ASCII font (characters 32 to 126)
// Each character is 5 bytes (each byte is 1 column of 7 vertical pixels, LSB at top)
extern const uint8_t font5x7[95][5];

// Large font for digits: 12x20 pixels
// Indices: 0-9 = digits '0'-'9', 10 = ':', 11 = '.', 12 = '%', 13 = 'C', 14 = '°' (degree)
// Stored as 12 columns x 3 vertical pages (36 bytes per character)
extern const uint8_t font12x20[15][36];

// Custom 8x8 status icons
extern const uint8_t icon_cpu[8];
extern const uint8_t icon_temp[8];
extern const uint8_t icon_disk[8];
extern const uint8_t icon_net[8];
extern const uint8_t icon_clock[8];
extern const uint8_t icon_ram[8];



// Additional icons and bold IP font
extern const uint8_t icon_eth[8];
extern const uint8_t icon_wifi[8];

// Medium 6x12 ASCII font (characters 32 to 126)
// 12 bytes per character (1 byte per row, 6 bits MSB-aligned)
extern const uint8_t font6x12[95][12];

// Bold 7x12 font for IP address: 11 glyphs (0-9 and index 10 = '.')
// 7 columns x 2 vertical pages (14 bytes per glyph)
extern const uint8_t font_bold_ip[11][14];



#endif // FONT_H
