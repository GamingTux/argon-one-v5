/*
 * argon-1v5-oled-legacy
 * High-performance C implementation of the original Argon ONE V5 OLED cards
 * and functions, enhanced with fast 1-second live value updates and zero flicker.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/i2c-dev.h>
#include <gpiod.h>

#include "legacy_assets.h"

#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_BUF_SIZE   1024
#define OLED_I2C_ADDR   0x3C
#define FAN_I2C_ADDR    0x1A

#define FONT_WD_SML     6
#define FONT_WD_REG     8
#define STD_LEFT_OFFSET 54

#define SHUTDOWN_FLAGFILE "/dev/shm/argonshutdownflag.txt"
#define SHM_FB_PATH       "/dev/shm/argon_oled_fb.bin"

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_oled_switch_req = 0;
static volatile sig_atomic_t g_reboot_req = 0;
static volatile sig_atomic_t g_shutdown_req = 0;
static bool g_oledswitch_only = true;

static int g_i2c_fd = -1;
static bool g_oled_powered = false;
static uint8_t g_oled_buf[OLED_BUF_SIZE];
static uint8_t g_last_flushed[OLED_BUF_SIZE];
static bool g_has_flushed = false;

// Runtime-modifiable copies of assets (initialized from embedded legacy_assets.h,
// optionally overridden by /etc/argon/oled/*.bin if present)
static uint8_t g_bg_cpu[OLED_BUF_SIZE];
static uint8_t g_bg_default[OLED_BUF_SIZE];
static uint8_t g_bg_ip[OLED_BUF_SIZE];
static uint8_t g_bg_raid[OLED_BUF_SIZE];
static uint8_t g_bg_ram[OLED_BUF_SIZE];
static uint8_t g_bg_storage[OLED_BUF_SIZE];
static uint8_t g_bg_temp[OLED_BUF_SIZE];
static uint8_t g_bg_time[OLED_BUF_SIZE];
static uint8_t g_bg_logo1v5[OLED_BUF_SIZE];
static uint8_t g_font_8x6[1536];
static uint8_t g_font_16x8[4096];

// Configuration state
typedef struct {
    bool enabled;
    int switch_duration;   // seconds (0 = manual only)
    int screensaver_sec;   // seconds (0 = disabled)
    int refresh_ms;        // live value refresh interval in ms (default 1000ms)
    char temp_unit;        // 'C' or 'F'
    char screen_list[16][16];
    int screen_count;
    time_t conf1_mtime;
    time_t conf2_mtime;
    time_t unit_mtime;
} LegacyConfig;

static LegacyConfig g_cfg;

// CPU Snapshot state for 30 Hz sliding-window live per-core usage
typedef struct {
    uint64_t total;
    uint64_t idle;
} CoreSnap;

#define MAX_CORES 16
#define CPU_RING_SIZE 10  // 10 frames @ 30Hz = ~333ms sliding window (~33 kernel ticks)
static CoreSnap g_cpu_ring[CPU_RING_SIZE][MAX_CORES];
static int g_cpu_ring_head = 0;
static int g_cpu_ring_count = 0;
static int g_stat_fd = -1;
static int g_temp_fd = -1;

static float g_live_core[MAX_CORES];
static int g_core_usage[MAX_CORES];
static float g_smooth_core[MAX_CORES];
static int g_num_cores = 0;
static float g_cur_temp = 40.0f;
static float g_smooth_temp = 40.0f;

// Storage item
typedef struct {
    char title[64];
    char value[16];
    int usage;
} StorageItem;

// RAID item
typedef struct {
    char title[16];
    char level[16];
    char state[32];
    char rebuild[32];
    uint64_t size_kb;
    int devices;
    int active;
    int working;
    int failed;
} RaidItem;

static void handle_sig(int sig) {
    (void)sig;
    g_running = 0;
}

static uint64_t get_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

// ============================================================================
// Asset Loading (Embedded + optional /etc/argon/oled/*.bin override)
// ============================================================================
static void load_bin_override(const char *path, uint8_t *dest, size_t len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    ssize_t r = read(fd, dest, len);
    (void)r;
    close(fd);
}

static void init_assets(void) {
    memcpy(g_bg_cpu,     asset_bgcpu,     OLED_BUF_SIZE);
    memcpy(g_bg_default, asset_bgdefault, OLED_BUF_SIZE);
    memcpy(g_bg_ip,      asset_bgip,      OLED_BUF_SIZE);
    memcpy(g_bg_raid,    asset_bgraid,    OLED_BUF_SIZE);
    memcpy(g_bg_ram,     asset_bgram,     OLED_BUF_SIZE);
    memcpy(g_bg_storage, asset_bgstorage, OLED_BUF_SIZE);
    memcpy(g_bg_temp,    asset_bgtemp,    OLED_BUF_SIZE);
    memcpy(g_bg_time,    asset_bgtime,    OLED_BUF_SIZE);
    memcpy(g_bg_logo1v5, asset_logo1v5,   OLED_BUF_SIZE);
    memcpy(g_font_8x6,   asset_font8x6,   sizeof(g_font_8x6));
    memcpy(g_font_16x8,  asset_font16x8,  sizeof(g_font_16x8));

    load_bin_override("/etc/argon/oled/bgcpu.bin",     g_bg_cpu,     OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/bgdefault.bin", g_bg_default, OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/bgip.bin",      g_bg_ip,      OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/bgraid.bin",    g_bg_raid,    OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/bgram.bin",     g_bg_ram,     OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/bgstorage.bin", g_bg_storage, OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/bgtemp.bin",    g_bg_temp,    OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/bgtime.bin",    g_bg_time,    OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/logo1v5.bin",   g_bg_logo1v5, OLED_BUF_SIZE);
    load_bin_override("/etc/argon/oled/font8x6.bin",   g_font_8x6,   sizeof(g_font_8x6));
    load_bin_override("/etc/argon/oled/font16x8.bin",  g_font_16x8,  sizeof(g_font_16x8));
}

// ============================================================================
// I2C SSD1306 Driver (Flicker-Free Direct GDDRAM Transfer)
// ============================================================================
static int oled_cmd(uint8_t cmd) {
    if (g_i2c_fd < 0) return -1;
    uint8_t buf[2] = { 0x00, cmd };
    return (write(g_i2c_fd, buf, 2) == 2) ? 0 : -1;
}

static void oled_power(bool on) {
    if (g_i2c_fd < 0) return;
    if (g_oled_powered == on) return;
    oled_cmd(on ? 0xAF : 0xAE);
    g_oled_powered = on;
}

static int oled_init_hw(void) {
    const char *devs[] = { "/dev/i2c-1", "/dev/i2c-0", "/dev/i2c-13", "/dev/i2c-14" };
    for (int i = 0; i < 4; i++) {
        int fd = open(devs[i], O_RDWR);
        if (fd >= 0) {
            if (ioctl(fd, I2C_SLAVE, OLED_I2C_ADDR) >= 0) {
                uint8_t test[2] = { 0x00, 0xE3 }; // NOP command
                if (write(fd, test, 2) == 2) {
                    g_i2c_fd = fd;
                    break;
                }
            }
            close(fd);
        }
    }
    if (g_i2c_fd < 0) return -1;

    static const uint8_t init_seq[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Set clock divide ratio
        0xA8, 0x3F, // Multiplex ratio 64
        0xD3, 0x00, // Display offset 0
        0x40,       // Start line 0
        0x8D, 0x14, // Charge pump enable
        0x20, 0x00, // Memory addressing mode: Horizontal
        0xA1,       // Segment re-map (col 127 -> SEG0)
        0xC8,       // COM output scan direction remapped
        0xDA, 0x12, // COM pins hardware config
        0x81, 0xCF, // Contrast
        0xD9, 0xF1, // Pre-charge period
        0xDB, 0x40, // VCOMH deselect level
        0xA4,       // Entire display ON from RAM
        0xA6,       // Normal display (not inverted)
        0xAF        // Display ON
    };
    for (size_t i = 0; i < sizeof(init_seq); i++) {
        oled_cmd(init_seq[i]);
    }
    g_oled_powered = true;
    return 0;
}

static void oled_set_window(uint8_t col_start, uint8_t col_end, uint8_t page_start, uint8_t page_end) {
    uint8_t cmd[7] = {
        0x00,       // Control byte: command stream
        0x21, col_start, col_end,
        0x22, page_start, page_end
    };
    ssize_t w = write(g_i2c_fd, cmd, sizeof(cmd));
    (void)w;
}

static void oled_send_data(const uint8_t *data, int len) {
    uint8_t chunk[65];
    chunk[0] = 0x40; // Control byte: GDDRAM data stream
    int offset = 0;
    while (offset < len) {
        int n = (len - offset > 64) ? 64 : (len - offset);
        memcpy(&chunk[1], &data[offset], n);
        ssize_t w = write(g_i2c_fd, chunk, n + 1);
        (void)w;
        offset += n;
    }
}

static void oled_flush(void) {
    if (g_i2c_fd < 0) {
        if (oled_init_hw() < 0) return;
    }

    // Fast path: skip both SHM write and I2C transfer if no pixel changed
    if (g_has_flushed && memcmp(g_oled_buf, g_last_flushed, OLED_BUF_SIZE) == 0) {
        return;
    }

    // Save shared-memory copy for preview/inspection when pixels change
    int shm_fd = open(SHM_FB_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (shm_fd >= 0) {
        ssize_t w = write(shm_fd, g_oled_buf, OLED_BUF_SIZE);
        (void)w;
        close(shm_fd);
    }

    if (!g_has_flushed) {
        oled_set_window(0, OLED_WIDTH - 1, 0, (OLED_HEIGHT / 8) - 1);
        oled_send_data(g_oled_buf, OLED_BUF_SIZE);
        memcpy(g_last_flushed, g_oled_buf, OLED_BUF_SIZE);
        g_has_flushed = true;
        return;
    }

    // Partial / Dirty-Region Scan across the 8 SSD1306 pages (0..7)
    int p_min_x[8], p_max_x[8];
    int global_min_p = 8, global_max_p = -1;
    int global_min_x = OLED_WIDTH, global_max_x = -1;
    int cost_spans = 0;

    for (int p = 0; p < 8; p++) {
        p_min_x[p] = OLED_WIDTH;
        p_max_x[p] = -1;
        const uint8_t *cur_row = &g_oled_buf[p * OLED_WIDTH];
        const uint8_t *old_row = &g_last_flushed[p * OLED_WIDTH];
        for (int x = 0; x < OLED_WIDTH; x++) {
            if (cur_row[x] != old_row[x]) {
                if (x < p_min_x[p]) p_min_x[p] = x;
                if (x > p_max_x[p]) p_max_x[p] = x;
            }
        }
        if (p_max_x[p] >= p_min_x[p]) {
            if (p < global_min_p) global_min_p = p;
            if (p > global_max_p) global_max_p = p;
            if (p_min_x[p] < global_min_x) global_min_x = p_min_x[p];
            if (p_max_x[p] > global_max_x) global_max_x = p_max_x[p];
            int span_len = p_max_x[p] - p_min_x[p] + 1;
            cost_spans += 7 + 1 + span_len;
        }
    }

    if (global_max_p < global_min_p) {
        return;
    }

    int bbox_w = global_max_x - global_min_x + 1;
    int bbox_p = global_max_p - global_min_p + 1;
    int bbox_bytes = bbox_w * bbox_p;
    int cost_bbox = 7 + bbox_bytes + ((bbox_bytes + 63) / 64);

    if (cost_spans <= cost_bbox) {
        // Transmit only the changed column span on each dirty page
        for (int p = global_min_p; p <= global_max_p; p++) {
            if (p_max_x[p] >= p_min_x[p]) {
                int span_len = p_max_x[p] - p_min_x[p] + 1;
                oled_set_window((uint8_t)p_min_x[p], (uint8_t)p_max_x[p], (uint8_t)p, (uint8_t)p);
                oled_send_data(&g_oled_buf[p * OLED_WIDTH + p_min_x[p]], span_len);
            }
        }
    } else {
        // Transmit single 2D bounding window
        uint8_t sub_buf[OLED_BUF_SIZE];
        int idx = 0;
        for (int p = global_min_p; p <= global_max_p; p++) {
            memcpy(&sub_buf[idx], &g_oled_buf[p * OLED_WIDTH + global_min_x], bbox_w);
            idx += bbox_w;
        }
        oled_set_window((uint8_t)global_min_x, (uint8_t)global_max_x, (uint8_t)global_min_p, (uint8_t)global_max_p);
        oled_send_data(sub_buf, idx);
    }

    memcpy(g_last_flushed, g_oled_buf, OLED_BUF_SIZE);
}

// ============================================================================
// Original Argon40 Drawing Primitives (Exact C Port)
// ============================================================================
static void oled_clearbuffer(uint8_t val) {
    memset(g_oled_buf, val ? 0xFF : 0x00, OLED_BUF_SIZE);
}

static void oled_loadbg(const char *bgname) {
    if (strcmp(bgname, "bgcpu") == 0)          memcpy(g_oled_buf, g_bg_cpu, OLED_BUF_SIZE);
    else if (strcmp(bgname, "bgdefault") == 0) memcpy(g_oled_buf, g_bg_default, OLED_BUF_SIZE);
    else if (strcmp(bgname, "bgip") == 0)      memcpy(g_oled_buf, g_bg_ip, OLED_BUF_SIZE);
    else if (strcmp(bgname, "bgraid") == 0)    memcpy(g_oled_buf, g_bg_raid, OLED_BUF_SIZE);
    else if (strcmp(bgname, "bgram") == 0)     memcpy(g_oled_buf, g_bg_ram, OLED_BUF_SIZE);
    else if (strcmp(bgname, "bgstorage") == 0) memcpy(g_oled_buf, g_bg_storage, OLED_BUF_SIZE);
    else if (strcmp(bgname, "bgtemp") == 0)    memcpy(g_oled_buf, g_bg_temp, OLED_BUF_SIZE);
    else if (strcmp(bgname, "bgtime") == 0)    memcpy(g_oled_buf, g_bg_time, OLED_BUF_SIZE);
    else if (strcmp(bgname, "logo1v5") == 0)   memcpy(g_oled_buf, g_bg_logo1v5, OLED_BUF_SIZE);
    else oled_clearbuffer(0);
}

static void oled_writebyterow(int x, int y, uint8_t bytevalue, int mode) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    int offset = OLED_WIDTH * (y >> 3) + x;
    if (mode == 0) {
        g_oled_buf[offset] = bytevalue;
    } else if (mode == 1) {
        g_oled_buf[offset] ^= bytevalue;
    } else {
        g_oled_buf[offset] |= bytevalue;
    }
}

static void oled_writebuffer(int x, int y, int value, int mode) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    int yoffset = y >> 3;
    int yshift = y & 0x07;
    uint8_t ybit = (uint8_t)(1 << yshift);
    uint8_t ymask = (uint8_t)(0xFF ^ ybit);
    uint8_t val_bit = value ? ybit : 0;
    int offset = OLED_WIDTH * yoffset + x;
    uint8_t curval = g_oled_buf[offset];
    if (mode & 1) {
        g_oled_buf[offset] = curval ^ val_bit;
    } else {
        g_oled_buf[offset] = (curval & ymask) | val_bit;
    }
}

static void oled_drawfilledrectangle(int x, int y, int wd, int ht, int mode) {
    if (wd <= 0 || ht <= 0) return;
    int ymax = y + ht;
    int cury = y & 0xF8;
    int xmax = x + wd;
    int curx;

    if ((y & 0x07) != 0) {
        int yshift = y & 0x07;
        uint8_t bytevalue = (uint8_t)((0xFF << yshift) & 0xFF);
        if (ymax - cury < 8) {
            int yshift2 = 8 - ((ymax - cury) & 0x07);
            bytevalue &= (uint8_t)(0xFF >> yshift2);
        }
        for (curx = x; curx < xmax; curx++) {
            oled_writebyterow(curx, cury, bytevalue, mode);
        }
        cury += 8;
    }

    while (cury + 8 <= ymax) {
        for (curx = x; curx < xmax; curx++) {
            oled_writebyterow(curx, cury, 0xFF, mode);
        }
        cury += 8;
    }

    if (cury < ymax) {
        int yshift = 8 - ((ymax - cury) & 0x07);
        uint8_t bytevalue = (uint8_t)(0xFF >> yshift);
        for (curx = x; curx < xmax; curx++) {
            oled_writebyterow(curx, cury, bytevalue, mode);
        }
    }
}

static void oled_writetext(const char *textdata, int x, int y, int charwd, int mode) {
    if (!textdata) return;
    if (charwd < 6) charwd = 6;
    int charht = ((charwd << 3) / 6);
    if (charht & 0x07) {
        charht = (charht & 0xF8) + 8;
    }

    const uint8_t *fontbytes = g_font_8x6;
    if (charht == 16 && charwd == 8) {
        fontbytes = g_font_16x8;
    } else {
        charht = 8;
        charwd = 6;
    }

    int numfontrow = charht >> 3;
    for (size_t ctr = 0; textdata[ctr] != '\0'; ctr++) {
        uint8_t ch = (uint8_t)textdata[ctr];
        // Support UTF-8 degree symbol 0xC2 0xB0 -> 0xA7 (167)
        if (ch == 0xC2 && (uint8_t)textdata[ctr + 1] == 0xB0) {
            ch = 167;
            ctr++;
        }
        int fontoffset = ((int)ch) * charwd;
        for (int fontcol = 0; fontcol < charwd && x < OLED_WIDTH; fontcol++) {
            if ((y & 0x07) == 0) {
                int row = y & 0xF8;
                for (int fontrow = 0; fontrow < numfontrow && row < OLED_HEIGHT && x >= 0; fontrow++) {
                    uint8_t curbyte = fontbytes[fontoffset + fontcol + (256 * charwd * fontrow)];
                    oled_writebyterow(x, row, curbyte, mode);
                    row += 8;
                }
            } else {
                int row = y;
                for (int fontrow = 0; fontrow < numfontrow && row < OLED_HEIGHT && x >= 0; fontrow++) {
                    uint8_t curbyte = fontbytes[fontoffset + fontcol + (256 * charwd * fontrow)];
                    for (int subrow = 0; subrow < 8 && row < OLED_HEIGHT; subrow++) {
                        int val = (curbyte & (1 << subrow)) ? 1 : 0;
                        oled_writebuffer(x, row, val, mode);
                        row++;
                    }
                }
            }
            x++;
        }
    }
}

static void oled_writetextaligned(const char *textdata, int x, int y, int boxwidth, int alignmode, int charwd, int mode) {
    if (!textdata) return;
    int len = (int)strlen(textdata);
    int leftoffset = 0;
    if (alignmode == 1) {
        leftoffset = (boxwidth - len * charwd) >> 1;
    } else if (alignmode == 2) {
        leftoffset = (boxwidth - len * charwd);
    }
    oled_writetext(textdata, x + leftoffset, y, charwd, mode);
}

// ============================================================================
// System Information (Direct C Kernel Interfaces - 0 Shell Spawning)
// ============================================================================
static void update_cpu_usage(void) {
    if (g_stat_fd < 0) {
        g_stat_fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
        if (g_stat_fd < 0) return;
    }

    char buf[2048];
    ssize_t n = pread(g_stat_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';

    int oldest_idx = (g_cpu_ring_count < CPU_RING_SIZE)
                     ? 0
                     : g_cpu_ring_head;

    int core_idx = 0;
    char *line_save = NULL;
    for (char *line = strtok_r(buf, "\n", &line_save); line; line = strtok_r(NULL, "\n", &line_save)) {
        if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
            int cid = atoi(&line[3]);
            if (cid < 0 || cid >= MAX_CORES) continue;

            char *p = strchr(line, ' ');
            if (!p) continue;
            while (*p == ' ') p++;

            uint64_t total = 0, idle = 0;
            int col = 1;
            char *saveptr = NULL;
            for (char *tok = strtok_r(p, " \t", &saveptr); tok; tok = strtok_r(NULL, " \t", &saveptr)) {
                uint64_t v = strtoull(tok, NULL, 10);
                if (col == 4 || col == 5) idle += v;
                total += v;
                col++;
            }

            if (g_cpu_ring_count > 0) {
                uint64_t prev_total = g_cpu_ring[oldest_idx][cid].total;
                uint64_t prev_idle  = g_cpu_ring[oldest_idx][cid].idle;
                if (total > prev_total) {
                    uint64_t dt = total - prev_total;
                    uint64_t di = (idle >= prev_idle) ? (idle - prev_idle) : 0;
                    uint64_t busy = (dt >= di) ? (dt - di) : 0;
                    float pct = (100.0f * (float)busy) / (float)dt;
                    if (pct < 0.0f) pct = 0.0f;
                    if (pct > 100.0f) pct = 100.0f;
                    g_live_core[cid] = pct;
                }
            } else {
                g_live_core[cid] = 0.0f;
            }

            g_cpu_ring[g_cpu_ring_head][cid].total = total;
            g_cpu_ring[g_cpu_ring_head][cid].idle  = idle;
            if (cid + 1 > core_idx) core_idx = cid + 1;
        }
    }

    if (core_idx > 0) {
        g_num_cores = core_idx;
        g_cpu_ring_head = (g_cpu_ring_head + 1) % CPU_RING_SIZE;
        if (g_cpu_ring_count < CPU_RING_SIZE) {
            g_cpu_ring_count++;
        }
    }
}

static float get_cpu_temp(void) {
    if (g_temp_fd < 0) {
        g_temp_fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY | O_CLOEXEC);
        if (g_temp_fd < 0) return 0.0f;
    }
    char buf[32];
    ssize_t n = pread(g_temp_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return 0.0f;
    buf[n] = '\0';
    int raw = atoi(buf);
    return (float)raw / 1000.0f;
}

static void get_ram_info(char *pct_out, size_t pct_sz, char *tot_out, size_t tot_sz) {
    uint64_t total_ram = 0, total_free = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            uint64_t val = 0;
            if (sscanf(line, "MemTotal: %llu", (unsigned long long *)&val) == 1) total_ram = val;
            else if (sscanf(line, "MemFree: %llu", (unsigned long long *)&val) == 1) total_free += val;
            else if (sscanf(line, "Buffers: %llu", (unsigned long long *)&val) == 1) total_free += val;
            else if (sscanf(line, "Cached: %llu", (unsigned long long *)&val) == 1) total_free += val;
        }
        fclose(fp);
    }
    if (total_ram == 0) {
        snprintf(pct_out, pct_sz, "0%%");
        snprintf(tot_out, tot_sz, "0GB");
        return;
    }
    int pct = (int)(100ULL * total_free / total_ram);
    int gb = (int)((total_ram + 512ULL * 1024ULL) >> 20);
    snprintf(pct_out, pct_sz, "%d%%", pct);
    snprintf(tot_out, tot_sz, "%dGB", gb);
}

static void format_kbstr(uint64_t kbval, char *out, size_t out_sz) {
    const char *suffixes[] = { "KB", "MB", "GB", "TB" };
    int sidx = 0;
    uint64_t rem = 0;
    while (kbval > 1023 && sidx < 3) {
        rem = kbval & 1023;
        kbval >>= 10;
        sidx++;
    }
    if (rem >= 500) kbval++;
    snprintf(out, out_sz, "%llu%s", (unsigned long long)kbval, suffixes[sidx]);
}

static void get_ip_addr(char *out, size_t out_sz) {
    snprintf(out, out_sz, "N/A");
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct sockaddr_in remote;
        memset(&remote, 0, sizeof(remote));
        remote.sin_family = AF_INET;
        remote.sin_port = htons(1);
        inet_pton(AF_INET, "254.255.255.255", &remote.sin_addr);
        if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) == 0) {
            struct sockaddr_in local;
            socklen_t len = sizeof(local);
            if (getsockname(sock, (struct sockaddr *)&local, &len) == 0) {
                inet_ntop(AF_INET, &local.sin_addr, out, out_sz);
            }
        }
        close(sock);
    }
}

static int list_raid(RaidItem *items, int max_items, char member_disks[16][16], int *member_count) {
    if (member_count) *member_count = 0;
    FILE *fp = fopen("/proc/mdstat", "r");
    if (!fp) return 0;

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp) && count < max_items) {
        if (strncmp(line, "Personalities", 13) == 0 || strncmp(line, "unused", 6) == 0) continue;
        char dev[16] = {0}, colon[4] = {0}, status[16] = {0}, rtype[16] = {0};
        if (sscanf(line, "%15s %3s %15s %15s", dev, colon, status, rtype) == 4 && strcmp(colon, ":") == 0) {
            RaidItem *it = &items[count];
            memset(it, 0, sizeof(*it));
            snprintf(it->title, sizeof(it->title), "%s", dev);
            snprintf(it->level, sizeof(it->level), "%s", rtype);
            snprintf(it->state, sizeof(it->state), "%s", status);

            // Parse member disks on this line
            char *p = strstr(line, rtype);
            if (p) {
                p += strlen(rtype);
                char *saveptr = NULL;
                for (char *tok = strtok_r(p, " \t\n", &saveptr); tok; tok = strtok_r(NULL, " \t\n", &saveptr)) {
                    char *br = strchr(tok, '[');
                    if (br) *br = '\0';
                    if (member_count && *member_count < 16) {
                        snprintf(member_disks[*member_count], 16, "%s", tok);
                        (*member_count)++;
                    }
                    it->devices++;
                    it->active++;
                    it->working++;
                }
            }

            // Read next line for blocks and [U/U]
            if (fgets(line, sizeof(line), fp)) {
                unsigned long long blocks = 0;
                if (sscanf(line, " %llu blocks", &blocks) == 1) {
                    it->size_kb = blocks;
                }
            }
            count++;
        }
    }
    fclose(fp);
    return count;
}

static int list_storage(StorageItem *items, int max_items) {
    RaidItem raids[4];
    char raid_members[16][16];
    int num_members = 0;
    int num_raids = list_raid(raids, 4, raid_members, &num_members);

    typedef struct {
        char dev[64];
        uint64_t used_kb;
        uint64_t total_kb;
    } DevAgg;
    DevAgg aggs[16];
    int agg_count = 0;

    // Track already-seen block device partitions so bind mounts aren't double-counted
    char seen_parts[32][64];
    int seen_count = 0;

    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return 0;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char spec[64], mnt[128], fstype[32];
        if (sscanf(line, "%63s %127s %31s", spec, mnt, fstype) != 3) continue;
        if (strncmp(spec, "/dev/", 5) != 0) continue;
        if (strncmp(spec, "/dev/loop", 9) == 0 || strncmp(spec, "/dev/ram", 8) == 0) continue;

        const char *part = spec + 5;
        bool already = false;
        for (int i = 0; i < seen_count; i++) {
            if (strcmp(seen_parts[i], part) == 0) { already = true; break; }
        }
        if (already) continue;
        if (seen_count < 32) {
            snprintf(seen_parts[seen_count++], 64, "%s", part);
        }

        // Check if member of RAID
        bool in_raid = false;
        for (int i = 0; i < num_members; i++) {
            if (strcmp(part, raid_members[i]) == 0) { in_raid = true; break; }
        }
        if (in_raid) continue;

        struct statvfs vfs;
        if (statvfs(mnt, &vfs) != 0 || vfs.f_blocks == 0) continue;

        uint64_t total_kb = ((uint64_t)vfs.f_blocks * (uint64_t)vfs.f_frsize) >> 10;
        uint64_t avail_kb = ((uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize) >> 10;
        uint64_t free_kb  = ((uint64_t)vfs.f_bfree  * (uint64_t)vfs.f_frsize) >> 10;
        uint64_t used_kb  = (total_kb >= free_kb) ? (total_kb - free_kb) : 0;
        (void)avail_kb;

        // Strip partition suffix just like argonsysinfo.py:
        // sdX1 -> sdX, hdX1 -> hdX, nvme0n1p1 -> nvme0n1, mmcblk0p1 -> mmcblk0
        char base[64];
        snprintf(base, sizeof(base), "%s", part);
        bool is_raid_dev = false;
        for (int i = 0; i < num_raids; i++) {
            if (strcmp(base, raids[i].title) == 0) { is_raid_dev = true; break; }
        }
        if (!is_raid_dev) {
            size_t blen = strlen(base);
            if ((strncmp(base, "sd", 2) == 0 || strncmp(base, "hd", 2) == 0) && blen > 2) {
                while (blen > 3 && base[blen - 1] >= '0' && base[blen - 1] <= '9') {
                    base[--blen] = '\0';
                }
            } else if (blen > 2) {
                base[blen - 2] = '\0';
            }
        }

        int found = -1;
        for (int i = 0; i < agg_count; i++) {
            if (strcmp(aggs[i].dev, base) == 0) { found = i; break; }
        }
        if (found >= 0) {
            aggs[found].used_kb += used_kb;
            aggs[found].total_kb += total_kb;
        } else if (agg_count < 16) {
            snprintf(aggs[agg_count].dev, sizeof(aggs[agg_count].dev), "%s", base);
            aggs[agg_count].used_kb = used_kb;
            aggs[agg_count].total_kb = total_kb;
            agg_count++;
        }
    }
    fclose(fp);

    int out_count = 0;
    for (int i = 0; i < agg_count && out_count < max_items; i++) {
        if (aggs[i].total_kb == 0) continue;
        snprintf(items[out_count].title, sizeof(items[out_count].title), "%.63s", aggs[i].dev);
        format_kbstr(aggs[i].total_kb, items[out_count].value, sizeof(items[out_count].value));
        items[out_count].usage = (int)(100ULL * aggs[i].used_kb / aggs[i].total_kb);
        out_count++;
    }
    return out_count;
}

// ============================================================================
// Configuration Loading (/etc/argononeoled.conf & /etc/argoneonoled.conf)
// ============================================================================
static void trim_str(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '"') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == '"')) {
        s[--len] = '\0';
    }
}

static void parse_oled_conf_file(const char *path, LegacyConfig *cfg) {
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        trim_str(key);
        trim_str(val);

        if (strcmp(key, "switchduration") == 0) {
            cfg->switch_duration = atoi(val);
        } else if (strcmp(key, "screensaver") == 0) {
            cfg->screensaver_sec = atoi(val);
        } else if (strcmp(key, "refreshinterval") == 0) {
            int sec = atoi(val);
            if (sec >= 1 && sec <= 60) cfg->refresh_ms = sec * 1000;
        } else if (strcmp(key, "enabled") == 0) {
            cfg->enabled = (val[0] != 'N' && val[0] != 'n');
        } else if (strcmp(key, "screenlist") == 0) {
            cfg->screen_count = 0;
            char *saveptr = NULL;
            for (char *tok = strtok_r(val, " \t", &saveptr); tok && cfg->screen_count < 16; tok = strtok_r(NULL, " \t", &saveptr)) {
                snprintf(cfg->screen_list[cfg->screen_count++], 16, "%s", tok);
            }
        }
    }
    fclose(fp);
}

static void reload_config_if_needed(bool force) {
    struct stat st1, st2, stu;
    time_t m1 = (stat("/etc/argononeoled.conf", &st1) == 0) ? st1.st_mtime : 0;
    time_t m2 = (stat("/etc/argoneonoled.conf", &st2) == 0) ? st2.st_mtime : 0;
    time_t mu = (stat("/etc/argonunits.conf",   &stu) == 0) ? stu.st_mtime : 0;

    if (!force && m1 == g_cfg.conf1_mtime && m2 == g_cfg.conf2_mtime && mu == g_cfg.unit_mtime) {
        return;
    }

    g_cfg.enabled = true;
    g_cfg.switch_duration = 10;
    g_cfg.screensaver_sec = 120;
    g_cfg.refresh_ms = 1000; // Fast 1s live refresh!
    g_cfg.temp_unit = 'C';

    const char *default_screens[] = { "clock", "cpu", "storage", "ram", "temp", "ip" };
    g_cfg.screen_count = 6;
    for (int i = 0; i < 6; i++) {
        snprintf(g_cfg.screen_list[i], 16, "%s", default_screens[i]);
    }

    // Load older config first, then newer config so whichever the user edited last wins!
    if (m1 > 0 && m2 > 0) {
        if (m1 >= m2) {
            parse_oled_conf_file("/etc/argoneonoled.conf", &g_cfg);
            parse_oled_conf_file("/etc/argononeoled.conf", &g_cfg);
        } else {
            parse_oled_conf_file("/etc/argononeoled.conf", &g_cfg);
            parse_oled_conf_file("/etc/argoneonoled.conf", &g_cfg);
        }
    } else if (m1 > 0) {
        parse_oled_conf_file("/etc/argononeoled.conf", &g_cfg);
    } else if (m2 > 0) {
        parse_oled_conf_file("/etc/argoneonoled.conf", &g_cfg);
    }

    FILE *ufp = fopen("/etc/argonunits.conf", "r");
    if (ufp) {
        char line[128];
        while (fgets(line, sizeof(line), ufp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = line, *v = eq + 1;
                trim_str(k);
                trim_str(v);
                if (strcmp(k, "temperature") == 0 && (v[0] == 'F' || v[0] == 'f')) {
                    g_cfg.temp_unit = 'F';
                }
            }
        }
        fclose(ufp);
    }

    g_cfg.conf1_mtime = m1;
    g_cfg.conf2_mtime = m2;
    g_cfg.unit_mtime = mu;
}

// ============================================================================
// Screen Rendering (100% Original Cards + Live Values)
// ============================================================================
static bool render_card(const char *screen_name, int page_subidx, int *out_pages) {
    if (out_pages) *out_pages = 1;

    if (strcmp(screen_name, "cpu") == 0) {
        oled_loadbg("bgcpu");
        int count = (g_num_cores > 0) ? g_num_cores : 4;
        if (count > 4) count = 4;
        int yoffset = 0;
        for (int i = 0; i < count; i++) {
            char line[32];
            int val = g_core_usage[i];
            snprintf(line, sizeof(line), "cpu%d: %d%%", i, val);
            oled_writetext(line, STD_LEFT_OFFSET, yoffset, FONT_WD_SML, 0);
            int bar_w = (int)(((OLED_WIDTH - STD_LEFT_OFFSET - 4) * g_smooth_core[i]) / 100.0f + 0.5f);
            oled_drawfilledrectangle(STD_LEFT_OFFSET, yoffset + 12, bar_w, 2, 0);
            yoffset += 16;
        }
        return true;

    } else if (strcmp(screen_name, "storage") == 0) {
        static StorageItem cached_items[12];
        static int cached_total = 0;
        static uint64_t last_storage_ms = 0;
        uint64_t now_ms = get_monotonic_ms();
        if (last_storage_ms == 0 || (now_ms - last_storage_ms) >= 1000ULL) {
            cached_total = list_storage(cached_items, 12);
            last_storage_ms = now_ms;
        }
        int total_items = cached_total;
        if (total_items <= 0) return false;

        int pages = (total_items + 2) / 3;
        if (out_pages) *out_pages = pages;
        int start_idx = (page_subidx % pages) * 3;

        oled_loadbg("bgstorage");
        int yoffset = 16;
        for (int i = 0; i < 3 && (start_idx + i) < total_items; i++) {
            StorageItem *it = &cached_items[start_idx + i];
            char pct_str[16];
            snprintf(pct_str, sizeof(pct_str), "%d%%", it->usage);
            oled_writetextaligned(it->value, 77, yoffset, OLED_WIDTH - 77, 2, FONT_WD_SML, 0);
            oled_writetextaligned(pct_str, 50, yoffset, 74 - 50, 2, FONT_WD_SML, 0);

            char short_name[9];
            snprintf(short_name, sizeof(short_name), "%.8s", it->title);
            oled_writetext(short_name, 0, yoffset, FONT_WD_SML, 0);
            yoffset += 16;
        }
        return true;

    } else if (strcmp(screen_name, "raid") == 0) {
        static RaidItem cached_raids[4];
        static int cached_raids_cnt = 0;
        static uint64_t last_raid_ms = 0;
        uint64_t now_ms = get_monotonic_ms();
        if (last_raid_ms == 0 || (now_ms - last_raid_ms) >= 1000ULL) {
            cached_raids_cnt = list_raid(cached_raids, 4, NULL, NULL);
            last_raid_ms = now_ms;
        }
        int total_raids = cached_raids_cnt;
        if (total_raids <= 0) return false;
        if (out_pages) *out_pages = total_raids;

        RaidItem *r = &cached_raids[page_subidx % total_raids];
        oled_loadbg("bgraid");
        oled_writetextaligned(r->title, 0, 0, STD_LEFT_OFFSET, 1, FONT_WD_SML, 0);
        oled_writetextaligned(r->level, 0, 8, STD_LEFT_OFFSET, 1, FONT_WD_SML, 0);
        char sz_str[16];
        format_kbstr(r->size_kb, sz_str, sizeof(sz_str));
        oled_writetextaligned(sz_str, 0, 56, STD_LEFT_OFFSET, 1, FONT_WD_SML, 0);

        if (r->state[0]) {
            oled_writetext(r->state, STD_LEFT_OFFSET, 8, FONT_WD_SML, 0);
        }
        if (r->rebuild[0]) {
            char rb[32];
            snprintf(rb, sizeof(rb), "Rebuild:%s", r->rebuild);
            oled_writetext(rb, STD_LEFT_OFFSET, 16, FONT_WD_SML, 0);
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "Active:%d/%d", r->active, r->devices);
        oled_writetext(buf, STD_LEFT_OFFSET, 32, FONT_WD_SML, 0);
        snprintf(buf, sizeof(buf), "Working:%d/%d", r->working, r->devices);
        oled_writetext(buf, STD_LEFT_OFFSET, 40, FONT_WD_SML, 0);
        snprintf(buf, sizeof(buf), "Failed:%d/%d", r->failed, r->devices);
        oled_writetext(buf, STD_LEFT_OFFSET, 48, FONT_WD_SML, 0);
        return true;

    } else if (strcmp(screen_name, "ram") == 0) {
        oled_loadbg("bgram");
        char pct[16], tot[16];
        get_ram_info(pct, sizeof(pct), tot, sizeof(tot));
        oled_writetextaligned(pct, STD_LEFT_OFFSET, 8, OLED_WIDTH - STD_LEFT_OFFSET, 1, FONT_WD_REG, 0);
        oled_writetextaligned("of", STD_LEFT_OFFSET, 24, OLED_WIDTH - STD_LEFT_OFFSET, 1, FONT_WD_REG, 0);
        oled_writetextaligned(tot, STD_LEFT_OFFSET, 40, OLED_WIDTH - STD_LEFT_OFFSET, 1, FONT_WD_REG, 0);
        return true;

    } else if (strcmp(screen_name, "temp") == 0) {
        oled_loadbg("bgtemp");
        float cval = g_cur_temp;
        char val_str[16];
        if (g_cfg.temp_unit == 'F') {
            float fval = 32.0f + (9.0f * cval) / 5.0f;
            snprintf(val_str, sizeof(val_str), "%.1f", fval);
            if (strlen(val_str) > 5) val_str[5] = '\0';
        } else {
            snprintf(val_str, sizeof(val_str), "%.1f", cval);
            if (strlen(val_str) > 4) val_str[4] = '\0';
        }
        char full_str[24];
        snprintf(full_str, sizeof(full_str), "%s\xA7%c", val_str, g_cfg.temp_unit);
        oled_writetextaligned(full_str, STD_LEFT_OFFSET, 24, OLED_WIDTH - STD_LEFT_OFFSET, 1, FONT_WD_REG, 0);

        // Calibrated to the 4 tick marks of bgtemp.bin:
        // 20C = 1px (y=40), 40C = 7px (y=34, tick 1), 50C = 10px (y=31, tick 2),
        // 60C = 13px (y=28, tick 3), 70C = 16px (y=25, tick 4), 85C = 21px (y=20, full)
        int maxht = 21;
        int barht = (int)(1.0f + (g_smooth_temp - 20.0f) * 0.3f + 0.5f);
        if (barht > maxht) barht = maxht;
        else if (barht < 1) barht = 1;
        oled_drawfilledrectangle(24, 20 + (maxht - barht), 3, barht, 0);
        return true;

    } else if (strcmp(screen_name, "ip") == 0) {
        static char cached_ip[32] = "N/A";
        static uint64_t last_ip_ms = 0;
        uint64_t now_ms = get_monotonic_ms();
        if (last_ip_ms == 0 || (now_ms - last_ip_ms) >= 1000ULL) {
            get_ip_addr(cached_ip, sizeof(cached_ip));
            last_ip_ms = now_ms;
        }
        oled_loadbg("bgip");
        oled_writetextaligned(cached_ip, 0, 8, OLED_WIDTH, 1, FONT_WD_REG, 0);
        return true;

    } else if (strcmp(screen_name, "logo1v5") == 0) {
        oled_loadbg("logo1v5");
        return true;

    } else {
        // Default: "clock" with live HH:MM:SS seconds display
        oled_loadbg("bgtime");
        static const char *weekday_names[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        static const char *month_names[] = {
            "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
        };
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        char md_str[16];
        if (tm_now.tm_mday < 10) {
            snprintf(md_str, sizeof(md_str), "%s %d", month_names[tm_now.tm_mon], tm_now.tm_mday);
        } else {
            snprintf(md_str, sizeof(md_str), "%s%d", month_names[tm_now.tm_mon], tm_now.tm_mday);
        }
        oled_writetextaligned(md_str, STD_LEFT_OFFSET, 8, OLED_WIDTH - STD_LEFT_OFFSET, 1, FONT_WD_REG, 0);
        oled_writetextaligned(weekday_names[tm_now.tm_wday], STD_LEFT_OFFSET, 24, OLED_WIDTH - STD_LEFT_OFFSET, 1, FONT_WD_REG, 0);

        char hms_str[16];
        snprintf(hms_str, sizeof(hms_str), "%02d:%02d:%02d", tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        oled_writetextaligned(hms_str, STD_LEFT_OFFSET, 40, OLED_WIDTH - STD_LEFT_OFFSET, 1, FONT_WD_REG, 0);
        return true;
    }
}

// ============================================================================
// GPIO4 Power & Page Switch Button Monitor Thread (libgpiod v2)
// ============================================================================
static void *button_monitor_thread(void *arg) {
    (void)arg;
    const char *chips[] = { "/dev/gpiochip4", "/dev/gpiochip0" };
    struct gpiod_chip *chip = NULL;
    for (int i = 0; i < 2; i++) {
        chip = gpiod_chip_open(chips[i]);
        if (chip) break;
    }
    if (!chip) return NULL;

    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_DOWN);

    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    unsigned int offsets[1] = { 4 };
    gpiod_line_config_add_line_settings(line_cfg, offsets, 1, settings);

    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "argon-one-v5-oled-legacy");

    struct gpiod_line_request *req = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if (!req) {
        gpiod_chip_close(chip);
        return NULL;
    }

    struct gpiod_edge_event_buffer *ev_buf = gpiod_edge_event_buffer_new(16);
    int fd = gpiod_line_request_get_fd(req);
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    uint64_t rise_ts_ns = 0;
    bool handled_pulse = false;

    while (g_running) {
        int ret = poll(&pfd, 1, 250);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            int n = gpiod_line_request_read_edge_events(req, ev_buf, 16);
            for (int i = 0; i < n; i++) {
                struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(ev_buf, i);
                int type = gpiod_edge_event_get_event_type(ev);
                uint64_t ts_ns = gpiod_edge_event_get_timestamp_ns(ev);

                if (type == GPIOD_EDGE_EVENT_RISING_EDGE) {
                    rise_ts_ns = ts_ns;
                    handled_pulse = false;

                    // Active 10ms polling while GPIO4 is HIGH (exact match with argonpowerbutton.py)
                    int pulsetime = 0;
                    while (gpiod_line_request_get_value(req, 4) == GPIOD_LINE_VALUE_ACTIVE && pulsetime < 500) {
                        usleep(10000); // 10ms
                        pulsetime++;
                    }
                    if (pulsetime > 0) {
                        handled_pulse = true;
                        if (g_oledswitch_only) {
                            if (pulsetime >= 1) {
                                g_oled_switch_req = 1;
                            }
                        } else {
                            if (pulsetime >= 2 && pulsetime <= 3) {
                                g_reboot_req = 1;
                                g_running = 0;
                            } else if (pulsetime >= 4 && pulsetime <= 5) {
                                g_shutdown_req = 1;
                                g_running = 0;
                            } else if (pulsetime >= 6) {
                                g_oled_switch_req = 1;
                            }
                        }
                    }
                } else if (type == GPIOD_EDGE_EVENT_FALLING_EDGE && rise_ts_ns > 0 && !handled_pulse) {
                    uint64_t pulse_ms = (ts_ns > rise_ts_ns) ? ((ts_ns - rise_ts_ns) / 1000000ULL) : 0;
                    if (pulse_ms >= 8) {
                        rise_ts_ns = 0;
                        if (g_oledswitch_only) {
                            g_oled_switch_req = 1;
                        } else {
                            if (pulse_ms >= 15 && pulse_ms <= 35) {
                                g_reboot_req = 1;
                                g_running = 0;
                            } else if (pulse_ms >= 36 && pulse_ms <= 55) {
                                g_shutdown_req = 1;
                                g_running = 0;
                            } else if (pulse_ms >= 56) {
                                g_oled_switch_req = 1;
                            }
                        }
                    }
                }
            }
        }
    }

    gpiod_edge_event_buffer_free(ev_buf);
    gpiod_line_request_release(req);
    gpiod_chip_close(chip);
    return NULL;
}

static void argon_mcu_signal_poweroff(void) {
    int fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) fd = open("/dev/i2c-0", O_RDWR);
    if (fd < 0) return;
    if (ioctl(fd, I2C_SLAVE, 0x1A) == 0) {
        // Send register-based poweroff (0x86 = 1) and legacy 0xFF poweroff
        uint8_t cmd_reg[2] = { 0x86, 0x01 };
        if (write(fd, cmd_reg, 2) < 0) {
            uint8_t cmd_legacy = 0xFF;
            ssize_t r = write(fd, &cmd_legacy, 1);
            (void)r;
        }
    }
    close(fd);
}

// ============================================================================
// Main Display & Service Loop
// ============================================================================
int main(int argc, char **argv) {
    if (argc > 1) {
        if (strcasecmp(argv[1], "SHUTDOWN") == 0 || strcasecmp(argv[1], "--shutdown") == 0 || strcasecmp(argv[1], "FANOFF") == 0) {
            if (strcasecmp(argv[1], "SHUTDOWN") == 0 || strcasecmp(argv[1], "--shutdown") == 0) {
                FILE *fp = fopen(SHUTDOWN_FLAGFILE, "w");
                if (fp) {
                    fputs("signalled", fp);
                    fclose(fp);
                }
                argon_mcu_signal_poweroff();
            }
            if (oled_init_hw() == 0) {
                oled_clearbuffer(0);
                oled_flush();
                oled_power(false);
                close(g_i2c_fd);
            }
            return 0;
        } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [SERVICE [OLEDSWITCH] | SHUTDOWN | --shutdown | FANOFF]\n", argv[0]);
            printf("Argon ONE V5 Legacy OLED Daemon (Original Cards + 30Hz Partial Refresh)\n");
            return 0;
        }
        if (argc > 2 && strcasecmp(argv[2], "OLEDSWITCH") != 0) {
            g_oledswitch_only = false;
        }
    }

    signal(SIGINT,  handle_sig);
    signal(SIGTERM, handle_sig);
    unlink(SHUTDOWN_FLAGFILE);

    init_assets();
    reload_config_if_needed(true);
    update_cpu_usage();
    g_cur_temp = get_cpu_temp();
    g_smooth_temp = g_cur_temp;
    for (int i = 0; i < MAX_CORES; i++) {
        g_smooth_core[i] = (float)g_core_usage[i];
    }

    if (oled_init_hw() < 0) {
        fprintf(stderr, "argon-one-v5-oled-legacy: Unable to initialize OLED on I2C 0x3C\n");
    }

    pthread_t btn_tid;
    bool has_btn_thread = (pthread_create(&btn_tid, NULL, button_monitor_thread, NULL) == 0);

    int screen_id = 0;
    int page_subidx = 0;
    bool screensaver_mode = false;
    uint64_t last_switch_ms = get_monotonic_ms();
    uint64_t screensaver_start_ms = last_switch_ms;
    uint64_t last_telemetry_ms = 0;

    // 30 Hz (33.33 ms) Render & Partial-Flush Loop
    while (g_running) {
        uint64_t frame_start_ms = get_monotonic_ms();

        if (access(SHUTDOWN_FLAGFILE, F_OK) == 0) {
            break;
        }

        reload_config_if_needed(false);

        // Handle button press (OLEDSWITCH) immediately (<33ms response)
        if (g_oled_switch_req) {
            g_oled_switch_req = 0;
            if (screensaver_mode) {
                screensaver_mode = false;
            }
            screen_id++;
            page_subidx = 0;
            if (g_cfg.screen_count > 0 && screen_id >= g_cfg.screen_count) {
                screen_id = 0;
            }
            screensaver_start_ms = frame_start_ms;
            last_switch_ms = frame_start_ms;
        }

        if (!g_cfg.enabled || g_cfg.screen_count == 0) {
            if (g_oled_powered) {
                oled_clearbuffer(0);
                oled_flush();
                oled_power(false);
            }
            usleep(100000);
            continue;
        }

        // Check auto-switch duration
        if (g_cfg.switch_duration > 0 && (frame_start_ms - last_switch_ms) >= (uint64_t)g_cfg.switch_duration * 1000ULL) {
            int total_subpages = 1;
            render_card(g_cfg.screen_list[screen_id], page_subidx, &total_subpages);
            page_subidx++;
            if (page_subidx >= total_subpages) {
                page_subidx = 0;
                screen_id = (screen_id + 1) % g_cfg.screen_count;
            }
            last_switch_ms = frame_start_ms;
            screensaver_start_ms = frame_start_ms;
            screensaver_mode = false;
        }

        // Check screensaver timeout
        if (g_cfg.screensaver_sec > 0 && !screensaver_mode &&
            (frame_start_ms - screensaver_start_ms) >= (uint64_t)g_cfg.screensaver_sec * 1000ULL) {
            screensaver_mode = true;
            oled_clearbuffer(0);
            oled_flush();
            oled_power(false);
        }

        if (!screensaver_mode) {
            // Sample CPU (30 Hz sliding ring-buffer) & temperature every frame (30x per second)
            update_cpu_usage();
            float live_temp = get_cpu_temp();

            // 30 Hz instant smooth bar tracking (EMA)
            for (int i = 0; i < MAX_CORES; i++) {
                g_smooth_core[i] += (g_live_core[i] - g_smooth_core[i]) * 0.45f;
            }
            g_smooth_temp += (live_temp - g_smooth_temp) * 0.45f;

            // Update readable numeric text digits 4x per second (every 250ms)
            if (last_telemetry_ms == 0 || (frame_start_ms - last_telemetry_ms) >= 250ULL) {
                for (int i = 0; i < MAX_CORES; i++) {
                    g_core_usage[i] = (int)(g_smooth_core[i] + 0.5f);
                }
                g_cur_temp = g_smooth_temp;
                last_telemetry_ms = frame_start_ms;
            }

            // Render active card at 30 Hz & transmit only dirty pixels over I2C
            int tries = 0;
            int total_subpages = 1;
            while (tries < g_cfg.screen_count) {
                if (screen_id >= g_cfg.screen_count) screen_id = 0;
                if (render_card(g_cfg.screen_list[screen_id], page_subidx, &total_subpages)) {
                    break;
                }
                screen_id = (screen_id + 1) % g_cfg.screen_count;
                page_subidx = 0;
                tries++;
            }

            oled_power(true);
            oled_flush();
        }

        uint64_t elapsed_ms = get_monotonic_ms() - frame_start_ms;
        if (elapsed_ms < 33) {
            usleep((useconds_t)((33 - elapsed_ms) * 1000ULL));
        }
    }

    // Clean shutdown of OLED
    oled_clearbuffer(0);
    oled_flush();
    oled_power(false);
    if (g_i2c_fd >= 0) close(g_i2c_fd);

    if (has_btn_thread) {
        pthread_join(btn_tid, NULL);
    }

    if (g_reboot_req) {
        int r = system("systemctl reboot");
        (void)r;
    } else if (g_shutdown_req) {
        int r = system("systemctl poweroff");
        (void)r;
    }

    return 0;
}
