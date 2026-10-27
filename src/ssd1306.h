#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

int  oled_init(const char *i2c_bus, uint8_t address);
void oled_close(void);
int  oled_power(bool on);
int  oled_set_contrast(uint8_t contrast);
int  oled_invert(bool invert);

void oled_clear(void);
void oled_pixel(int x, int y, int color);
void oled_line(int x0, int y0, int x1, int y1, int color);
void oled_rect(int x, int y, int w, int h, bool fill, int color);

void oled_draw_char(int x, int y, char c, int color);
void oled_draw_string(int x, int y, const char *str, int color);
void oled_draw_med_char(int x, int y, char c, int color);
void oled_draw_med_string(int x, int y, const char *str, int color);
void oled_draw_large_char(int x, int y, char c, int color);
void oled_draw_large_string(int x, int y, const char *str, int color);
void oled_draw_badge(int x, int y, const char *text, int pad_x, int pad_y, bool med_font);
void oled_draw_icon(int x, int y, const uint8_t *icon, int color);

void oled_draw_sparkline(int x, int y, int w, int h, const float *data, int count, float min_v, float max_v);
void oled_draw_progress_bar(int x, int y, int w, int h, float percent);
void oled_draw_equalizer_bars(int x, int y, int h, const int *heights, const int *peaks, int num_cols);

void oled_set_pixel_shift(int dx, int dy);
int  oled_flush(void);
const uint8_t *oled_get_fb(void);



// Bold IP and clipped string drawing
int oled_draw_bold_ip(int x, int y, const char *ip_str, int color);
int oled_calc_bold_ip_width(const char *ip_str);
void oled_draw_string_clipped(int x, int y, const char *str, int clip_x0, int clip_x1, int color);
void oled_draw_down_arrow(int x, int y, int color);
void oled_draw_up_arrow(int x, int y, int color);



#endif // SSD1306_H
