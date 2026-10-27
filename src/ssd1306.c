#include "ssd1306.h"
#include "font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

static int i2c_fd = -1;
static uint8_t fb[1024];
static int shift_x = 0;
static int shift_y = 0;

static int oled_cmd(uint8_t cmd) {
    if (i2c_fd < 0) return -1;
    uint8_t buf[2] = { 0x00, cmd };
    return write(i2c_fd, buf, 2);
}

int oled_init(const char *i2c_bus, uint8_t address) {
    if (i2c_fd >= 0) close(i2c_fd);

    i2c_fd = open(i2c_bus, O_RDWR);
    if (i2c_fd < 0) {
        perror("oled: open i2c bus failed");
        return -1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, address) < 0) {
        perror("oled: ioctl I2C_SLAVE failed");
        close(i2c_fd);
        i2c_fd = -1;
        return -1;
    }

    static const uint8_t init_sequence[] = {
        0xAE,       // Display off
        0xD5, 0x80, // Clock divide / oscillator freq
        0xA8, 0x3F, // Multiplex 64 lines
        0xD3, 0x00, // Display offset 0
        0x40,       // Start line 0
        0x8D, 0x14, // Charge pump ON
        0x20, 0x00, // Horizontal addressing mode
        0xA1,       // Segment remap (col 127 is SEG0)
        0xC8,       // COM scan direction (remapped)
        0xDA, 0x12, // COM pins hardware config
        0x81, 0xCF, // Initial contrast
        0xD9, 0xF1, // Pre-charge period
        0xDB, 0x40, // Vcomh deselect level
        0xA4,       // Output follows RAM
        0xA6,       // Normal display (non-inverted)
        0xAF        // Display ON
    };

    for (size_t i = 0; i < sizeof(init_sequence); i++) {
        if (oled_cmd(init_sequence[i]) < 0) {
            close(i2c_fd);
            i2c_fd = -1;
            return -1;
        }
    }

    oled_clear();
    oled_flush();
    return 0;
}

void oled_close(void) {
    if (i2c_fd >= 0) {
        oled_power(false);
        close(i2c_fd);
        i2c_fd = -1;
    }
}

int oled_power(bool on) {
    return oled_cmd(on ? 0xAF : 0xAE);
}

int oled_set_contrast(uint8_t contrast) {
    if (oled_cmd(0x81) < 0) return -1;
    return oled_cmd(contrast);
}

int oled_invert(bool invert) {
    return oled_cmd(invert ? 0xA7 : 0xA6);
}

void oled_clear(void) {
    memset(fb, 0, sizeof(fb));
}

void oled_set_pixel_shift(int dx, int dy) {
    shift_x = dx;
    shift_y = dy;
}

void oled_pixel(int x, int y, int color) {
    x += shift_x;
    y += shift_y;
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;

    int idx = (y / 8) * OLED_WIDTH + x;
    uint8_t mask = (1 << (y % 8));

    if (color) {
        fb[idx] |= mask;
    } else {
        fb[idx] &= ~mask;
    }
}

void oled_line(int x0, int y0, int x1, int y1, int color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        oled_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void oled_rect(int x, int y, int w, int h, bool fill, int color) {
    if (fill) {
        for (int i = 0; i < h; i++) {
            oled_line(x, y + i, x + w - 1, y + i, color);
        }
    } else {
        oled_line(x, y, x + w - 1, y, color);
        oled_line(x, y + h - 1, x + w - 1, y + h - 1, color);
        oled_line(x, y, x, y + h - 1, color);
        oled_line(x + w - 1, y, x + w - 1, y + h - 1, color);
    }
}

void oled_draw_char(int x, int y, char c, int color) {
    if (c == '^' || (unsigned char)c == 0xB0) {
        // Draw crisp 4x4 degree symbol '°'
        oled_pixel(x + 1, y, color);
        oled_pixel(x + 2, y, color);
        oled_pixel(x, y + 1, color);
        oled_pixel(x + 3, y + 1, color);
        oled_pixel(x, y + 2, color);
        oled_pixel(x + 3, y + 2, color);
        oled_pixel(x + 1, y + 3, color);
        oled_pixel(x + 2, y + 3, color);
        return;
    }

    if (c < 32 || c > 126) c = '?';
    int font_idx = c - 32;

    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[font_idx][col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                oled_pixel(x + col, y + row, color);
            }
        }
    }
}

void oled_draw_string(int x, int y, const char *str, int color) {
    while (*str) {
        if ((unsigned char)str[0] == 0xC2 && (unsigned char)str[1] == 0xB0) {
            // UTF-8 degree symbol '°'
            oled_draw_char(x, y, '^', color);
            x += 5;
            str += 2;
            continue;
        }
        oled_draw_char(x, y, *str, color);
        x += (*str == '^') ? 5 : 6; // 5 pixels + 1 pixel space
        str++;
    }
}

void oled_draw_med_char(int x, int y, char c, int color) {
    if (c < 32 || c > 126) c = '?';
    int font_idx = c - 32;
    for (int row = 0; row < 12; row++) {
        uint8_t byte = font6x12[font_idx][row];
        for (int col = 0; col < 6; col++) {
            if (byte & (0x80 >> col)) {
                oled_pixel(x + col, y + row, color);
            }
        }
    }
}

void oled_draw_med_string(int x, int y, const char *str, int color) {
    while (*str) {
        oled_draw_med_char(x, y, *str, color);
        x += 7; // 6px char + 1px space
        str++;
    }
}

void oled_draw_badge(int x, int y, const char *text, int pad_x, int pad_y, bool med_font) {
    if (!text || !*text) return;
    int len = (int)strlen(text);
    int char_w = med_font ? 7 : 6;
    int text_w = len * char_w - 1;
    int text_h = med_font ? 12 : 7;
    int badge_w = text_w + 2 * pad_x;
    int badge_h = text_h + 2 * pad_y;

    oled_rect(x, y, badge_w, badge_h, true, 1);
    if (med_font) {
        oled_draw_med_string(x + pad_x, y + pad_y, text, 0);
    } else {
        oled_draw_string(x + pad_x, y + pad_y, text, 0);
    }
}

void oled_draw_large_char(int x, int y, char c, int color) {
    int idx = -1;
    if (c >= '0' && c <= '9') idx = c - '0';
    else if (c == ':') idx = 10;
    else if (c == '.') idx = 11;
    else if (c == '%') idx = 12;
    else if (c == 'C' || c == 'c') idx = 13;
    else if (c == '^') idx = 14; // degree symbol
    if (idx < 0) return;

    for (int page = 0; page < 3; page++) {
        for (int col = 0; col < 12; col++) {
            uint8_t byte = font12x20[idx][page * 12 + col];
            for (int bit = 0; bit < 8; bit++) {
                int py = y + page * 8 + bit;
                if (py < y + 24 && (byte & (1 << bit))) {
                    oled_pixel(x + col, py, color);
                }
            }
        }
    }
}

void oled_draw_large_string(int x, int y, const char *str, int color) {
    while (*str) {
        oled_draw_large_char(x, y, *str, color);
        x += (*str == ':' || *str == '.') ? 7 : 12;
        str++;
    }
}

void oled_draw_icon(int x, int y, const uint8_t *icon, int color) {
    for (int col = 0; col < 8; col++) {
        uint8_t byte = icon[col];
        for (int row = 0; row < 8; row++) {
            if (byte & (1 << row)) {
                oled_pixel(x + col, y + row, color);
            }
        }
    }
}

void oled_draw_sparkline(int x, int y, int w, int h, const float *data, int count, float min_v, float max_v) {
    if (count < 2 || w < 2 || h < 2) return;

    // Draw baseline bounding dotted/thin box
    oled_line(x, y + h - 1, x + w - 1, y + h - 1, 1);

    if (max_v <= min_v) max_v = min_v + 1.0f;

    int prev_px = -1, prev_py = -1;
    int step = (count > w) ? (count / w) : 1;

    for (int i = 0; i < count; i += step) {
        int px = x + (i * (w - 1)) / (count - 1);
        float val = data[i];
        if (val < min_v) val = min_v;
        if (val > max_v) val = max_v;

        int py = y + h - 1 - (int)(((val - min_v) / (max_v - min_v)) * (h - 2));

        if (prev_px >= 0) {
            oled_line(prev_px, prev_py, px, py, 1);
        } else {
            oled_pixel(px, py, 1);
        }
        prev_px = px;
        prev_py = py;
    }
}

void oled_draw_progress_bar(int x, int y, int w, int h, float percent) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    oled_rect(x, y, w, h, false, 1); // Border
    int fill_w = (int)(((w - 4) * percent) / 100.0f);
    if (fill_w > 0) {
        oled_rect(x + 2, y + 2, fill_w, h - 4, true, 1);
    }
}

void oled_draw_equalizer_bars(int x, int y, int h, const int *heights, const int *peaks, int num_cols) {
    if (!heights || num_cols <= 0) return;
    for (int c = 0; c < num_cols; c++) {
        int cx = x + c * 4;
        int ht = heights[c];
        if (ht < 1) ht = 1;
        if (ht > h) ht = h;
        // Solid bar from bottom up
        oled_rect(cx, y + h - ht, 3, ht, true, 1);
        // Floating peak dot if above bar
        if (peaks) {
            int pk = peaks[c];
            if (pk > ht && pk <= h) {
                oled_rect(cx, y + h - pk, 3, 1, true, 1);
            }
        }
    }
}

const uint8_t *oled_get_fb(void) {
    return fb;
}

int oled_flush(void) {
    // Write framebuffer to shared memory for live desktop simulation/preview
    int shm = open("/dev/shm/argon_oled_fb.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (shm >= 0) {
        write(shm, fb, sizeof(fb));
        close(shm);
    }

    if (i2c_fd < 0) return -1;

    // Set Column Address (0 to 127)
    oled_cmd(0x21); oled_cmd(0); oled_cmd(127);
    // Set Page Address (0 to 7)
    oled_cmd(0x22); oled_cmd(0); oled_cmd(7);

    // Send 1024 bytes in 64-byte chunks with 0x40 data prefix
    uint8_t chunk[65];
    chunk[0] = 0x40;

    for (int offset = 0; offset < 1024; offset += 64) {
        memcpy(&chunk[1], &fb[offset], 64);
        if (write(i2c_fd, chunk, 65) != 65) {
            return -1;
        }
    }
    return 0;
}

int oled_draw_bold_ip(int x, int y, const char *ip_str, int color) {
    int cur_x = x;
    while (*ip_str) {
        char c = *ip_str++;
        int idx = -1;
        int w = 7;
        if (c >= '0' && c <= '9') {
            idx = c - '0';
            w = 7;
        } else if (c == '.') {
            idx = 10;
            w = 3;
        }
        if (idx < 0) continue;

        for (int col = 0; col < w; col++) {
            uint8_t low = font_bold_ip[idx][col];
            uint8_t high = font_bold_ip[idx][7 + col];
            for (int r = 0; r < 8; r++) {
                if (low & (1 << r)) {
                    oled_pixel(cur_x + col, y + r, color);
                }
            }
            for (int r = 0; r < 4; r++) {
                if (high & (1 << r)) {
                    oled_pixel(cur_x + col, y + 8 + r, color);
                }
            }
        }
        cur_x += w + 1;
    }
    return cur_x - x;
}

int oled_calc_bold_ip_width(const char *ip_str) {
    int w = 0;
    while (*ip_str) {
        char c = *ip_str++;
        if (c >= '0' && c <= '9') w += 8;
        else if (c == '.') w += 4;
    }
    return (w > 0) ? (w - 1) : 0;
}

void oled_draw_string_clipped(int x, int y, const char *str, int clip_x0, int clip_x1, int color) {
    int cur_x = x;
    while (*str) {
        char c = *str++;
        if (c < 32 || c > 126) continue;
        const uint8_t *glyph = font5x7[c - 32];
        for (int col = 0; col < 5; col++) {
            int px = cur_x + col;
            if (px >= clip_x0 && px <= clip_x1) {
                uint8_t b = glyph[col];
                for (int row = 0; row < 7; row++) {
                    if (b & (1 << row)) {
                        oled_pixel(px, y + row, color);
                    }
                }
            }
        }
        cur_x += 6;
    }
}

void oled_draw_down_arrow(int x, int y, int color) {
    oled_line(x, y, x + 6, y, color);
    oled_line(x + 1, y + 1, x + 5, y + 1, color);
    oled_line(x + 2, y + 2, x + 4, y + 2, color);
    oled_pixel(x + 3, y + 3, color);
}

void oled_draw_up_arrow(int x, int y, int color) {
    oled_pixel(x + 3, y, color);
    oled_line(x + 2, y + 1, x + 4, y + 1, color);
    oled_line(x + 1, y + 2, x + 5, y + 2, color);
    oled_line(x, y + 3, x + 6, y + 3, color);
}
