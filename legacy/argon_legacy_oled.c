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

// CPU Snapshot state for non-blocking live per-core usage
typedef struct {
    uint64_t total;
    uint64_t idle;
} CoreSnap;

#define MAX_CORES 16
static CoreSnap g_prev_cores[MAX_CORES];
static int g_core_usage[MAX_CORES];
static int g_num_cores = 0;
static bool g_cpu_initialized = false;

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

static void oled_flush(void) {
    // Save shared-memory copy for preview/inspection
    int shm_fd = open(SHM_FB_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (shm_fd >= 0) {
        ssize_t w = write(shm_fd, g_oled_buf, OLED_BUF_SIZE);
        (void)w;
        close(shm_fd);
    }

    if (g_i2c_fd < 0) {
        if (oled_init_hw() < 0) return;
    }

    // Skip I2C transfer if buffer is identical to last frame
    if (g_has_flushed && memcmp(g_oled_buf, g_last_flushed, OLED_BUF_SIZE) == 0) {
        return;
    }

    oled_cmd(0x21); // Column address
    oled_cmd(0);
    oled_cmd(OLED_WIDTH - 1);
    oled_cmd(0x22); // Page address
    oled_cmd(0);
    oled_cmd((OLED_HEIGHT / 8) - 1);

    uint8_t chunk[65];
    chunk[0] = 0x40; // Co=0, D/C#=1
    for (int offset = 0; offset < OLED_BUF_SIZE; offset += 64) {
        memcpy(&chunk[1], &g_oled_buf[offset], 64);
        ssize_t w = write(g_i2c_fd, chunk, 65);
        (void)w;
    }
    memcpy(g_last_flushed, g_oled_buf, OLED_BUF_SIZE);
    g_has_flushed = true;
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
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;

    char line[256];
    int core_idx = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
            int cid = atoi(&line[3]);
            if (cid < 0 || cid >= MAX_CORES) continue;

            char *p = strchr(line, ' ');
            if (!p) continue;
            while (*p == ' ') p++;

            uint64_t total = 0, idle = 0;
            int col = 1;
            char *saveptr = NULL;
            for (char *tok = strtok_r(p, " \t\n", &saveptr); tok; tok = strtok_r(NULL, " \t\n", &saveptr)) {
                uint64_t v = strtoull(tok, NULL, 10);
                if (col == 4 || col == 5) idle += v;
                total += v;
                col++;
            }

            if (g_cpu_initialized && total > g_prev_cores[cid].total) {
                uint64_t dt = total - g_prev_cores[cid].total;
                uint64_t di = idle - g_prev_cores[cid].idle;
                g_core_usage[cid] = (int)(100ULL * (dt - di) / dt);
                if (g_core_usage[cid] < 0) g_core_usage[cid] = 0;
                if (g_core_usage[cid] > 100) g_core_usage[cid] = 100;
            } else if (!g_cpu_initialized) {
                g_core_usage[cid] = 0;
            }
            g_prev_cores[cid].total = total;
            g_prev_cores[cid].idle = idle;
            if (cid + 1 > core_idx) core_idx = cid + 1;
        }
    }
    fclose(fp);
    if (core_idx > 0) {
        g_num_cores = core_idx;
        g_cpu_initialized = true;
    }
}

static float get_cpu_temp(void) {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return 0.0f;
    int raw = 0;
    if (fscanf(fp, "%d", &raw) != 1) raw = 0;
    fclose(fp);
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
            int bar_w = ((OLED_WIDTH - STD_LEFT_OFFSET - 4) * val) / 100;
            oled_drawfilledrectangle(STD_LEFT_OFFSET, yoffset + 12, bar_w, 2, 0);
            yoffset += 16;
        }
        return true;

    } else if (strcmp(screen_name, "storage") == 0) {
        StorageItem items[12];
        int total_items = list_storage(items, 12);
        if (total_items <= 0) return false;

        int pages = (total_items + 2) / 3;
        if (out_pages) *out_pages = pages;
        int start_idx = (page_subidx % pages) * 3;

        oled_loadbg("bgstorage");
        int yoffset = 16;
        for (int i = 0; i < 3 && (start_idx + i) < total_items; i++) {
            StorageItem *it = &items[start_idx + i];
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
        RaidItem raids[4];
        int total_raids = list_raid(raids, 4, NULL, NULL);
        if (total_raids <= 0) return false;
        if (out_pages) *out_pages = total_raids;

        RaidItem *r = &raids[page_subidx % total_raids];
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
        float cval = get_cpu_temp();
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

        // Thermometer mercury bar (40C min .. 80C max, height 1..21)
        int maxht = 21;
        int barht = (int)(maxht * (cval - 40.0f) / 40.0f);
        if (barht > maxht) barht = maxht;
        else if (barht < 1) barht = 1;
        oled_drawfilledrectangle(24, 20 + (maxht - barht), 3, barht, 2);
        return true;

    } else if (strcmp(screen_name, "ip") == 0) {
        oled_loadbg("bgip");
        char ip[32];
        get_ip_addr(ip, sizeof(ip));
        oled_writetextaligned(ip, 0, 8, OLED_WIDTH, 1, FONT_WD_REG, 0);
        return true;

    } else if (strcmp(screen_name, "logo1v5") == 0) {
        oled_loadbg("logo1v5");
        return true;

    } else {
        // Default: "clock"
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

        char hm_str[16];
        snprintf(hm_str, sizeof(hm_str), "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
        oled_writetextaligned(hm_str, STD_LEFT_OFFSET, 40, OLED_WIDTH - STD_LEFT_OFFSET, 1, FONT_WD_REG, 0);
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
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_DISABLED);

    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    unsigned int offsets[1] = { 4 };
    gpiod_line_config_add_line_settings(line_cfg, offsets, 1, settings);

    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "argon-1v5-oled-legacy");

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
    uint64_t high_start_ms = 0;

    while (g_running) {
        int ret = poll(&pfd, 1, 500);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            int n = gpiod_line_request_read_edge_events(req, ev_buf, 16);
            for (int i = 0; i < n; i++) {
                struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(ev_buf, i);
                int type = gpiod_edge_event_get_event_type(ev);
                if (type == GPIOD_EDGE_EVENT_RISING_EDGE) {
                    high_start_ms = get_monotonic_ms();
                } else if (type == GPIOD_EDGE_EVENT_FALLING_EDGE && high_start_ms > 0) {
                    uint64_t pulse_ms = get_monotonic_ms() - high_start_ms;
                    high_start_ms = 0;
                    // Original Argon ONE MCU pulse encoding on GPIO4:
                    // 20-30ms = double-tap reboot
                    // 40-50ms = 3s hold shutdown
                    // >= 60ms = OLED page switch
                    if (pulse_ms >= 15 && pulse_ms <= 35) {
                        g_reboot_req = 1;
                        g_running = 0;
                    } else if (pulse_ms >= 36 && pulse_ms <= 55) {
                        g_shutdown_req = 1;
                        g_running = 0;
                    } else if (pulse_ms >= 60) {
                        g_oled_switch_req = 1;
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

static void write_sysfs_str(const char *path, const char *val) {
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        ssize_t w = write(fd, val, strlen(val));
        (void)w;
        close(fd);
    }
}

static void apply_safe_perf_tuning(void) {
    // 1. Fast CPU frequency ramp-up on load while preserving 1.5 GHz cool idle
    write_sysfs_str("/sys/devices/system/cpu/cpufreq/ondemand/up_threshold", "50");
    write_sysfs_str("/sys/devices/system/cpu/cpufreq/ondemand/sampling_down_factor", "2");
    write_sysfs_str("/sys/devices/system/cpu/cpufreq/ondemand/io_is_busy", "1");
    write_sysfs_str("/sys/devices/system/cpu/cpufreq/policy0/schedutil/rate_limit_us", "500");

    // 2. Direct multiqueue ('none') & read-ahead for NVMe drives
    write_sysfs_str("/sys/block/nvme0n1/queue/scheduler", "none");
    write_sysfs_str("/sys/block/nvme0n1/queue/read_ahead_kb", "512");
    write_sysfs_str("/sys/block/nvme0n1/queue/rq_affinity", "2");
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
            printf("Argon ONE V5 Legacy OLED Daemon (Original Cards + Fast 1s Refresh)\n");
            return 0;
        }
    }

    signal(SIGINT,  handle_sig);
    signal(SIGTERM, handle_sig);
    unlink(SHUTDOWN_FLAGFILE);

    init_assets();
    apply_safe_perf_tuning();
    reload_config_if_needed(true);
    update_cpu_usage();

    if (oled_init_hw() < 0) {
        fprintf(stderr, "argon-1v5-oled-legacy: Unable to initialize OLED on I2C 0x3C\n");
    }

    pthread_t btn_tid;
    bool has_btn_thread = (pthread_create(&btn_tid, NULL, button_monitor_thread, NULL) == 0);

    int screen_id = 0;
    int page_subidx = 0;
    bool screensaver_mode = false;
    uint64_t last_switch_ms = get_monotonic_ms();
    uint64_t screensaver_start_ms = last_switch_ms;
    uint64_t last_refresh_ms = 0;

    while (g_running) {
        if (access(SHUTDOWN_FLAGFILE, F_OK) == 0) {
            break;
        }

        reload_config_if_needed(false);

        uint64_t now_ms = get_monotonic_ms();

        // Handle button press (OLEDSWITCH)
        if (g_oled_switch_req) {
            g_oled_switch_req = 0;
            if (screensaver_mode) {
                // Wake up from screensaver without jumping page first, or advance as original
                screensaver_mode = false;
            }
            screen_id++;
            page_subidx = 0;
            if (g_cfg.screen_count > 0 && screen_id >= g_cfg.screen_count) {
                screen_id = 0;
            }
            screensaver_start_ms = now_ms;
            last_switch_ms = now_ms;
            last_refresh_ms = 0; // Trigger immediate redraw
        }

        if (!g_cfg.enabled || g_cfg.screen_count == 0) {
            if (g_oled_powered) {
                oled_clearbuffer(0);
                oled_flush();
                oled_power(false);
            }
            usleep(200000);
            continue;
        }

        // Check auto-switch duration
        if (g_cfg.switch_duration > 0 && (now_ms - last_switch_ms) >= (uint64_t)g_cfg.switch_duration * 1000ULL) {
            int total_subpages = 1;
            render_card(g_cfg.screen_list[screen_id], page_subidx, &total_subpages);
            page_subidx++;
            if (page_subidx >= total_subpages) {
                page_subidx = 0;
                screen_id = (screen_id + 1) % g_cfg.screen_count;
            }
            last_switch_ms = now_ms;
            // In auto-switch mode, original Argon script resets screensaver counter on page switch
            screensaver_start_ms = now_ms;
            screensaver_mode = false;
            last_refresh_ms = 0;
        }

        // Check screensaver timeout
        if (g_cfg.screensaver_sec > 0 && !screensaver_mode &&
            (now_ms - screensaver_start_ms) >= (uint64_t)g_cfg.screensaver_sec * 1000ULL) {
            screensaver_mode = true;
            oled_clearbuffer(0);
            oled_flush();
            oled_power(false);
        }

        // Live refresh of current screen every refresh_ms (default 1000ms)
        if (!screensaver_mode && (last_refresh_ms == 0 || (now_ms - last_refresh_ms) >= (uint64_t)g_cfg.refresh_ms)) {
            update_cpu_usage();

            int tries = 0;
            int total_subpages = 1;
            while (tries < g_cfg.screen_count) {
                if (screen_id >= g_cfg.screen_count) screen_id = 0;
                if (render_card(g_cfg.screen_list[screen_id], page_subidx, &total_subpages)) {
                    break;
                }
                // If card has no data (e.g. "raid" when no RAID exists), advance to next card
                screen_id = (screen_id + 1) % g_cfg.screen_count;
                page_subidx = 0;
                tries++;
            }

            oled_power(true);
            oled_flush();
            last_refresh_ms = now_ms;
        }

        usleep(50000); // 50ms responsive event polling
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
