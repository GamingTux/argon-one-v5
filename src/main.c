#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#include "ssd1306.h"
#include "telemetry.h"
#include "button.h"
#include "config.h"
#include "font.h"

#define SOCKET_PATH "/run/argon-v5-oled.sock"

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_next_screen = 0;
static volatile sig_atomic_t g_prev_screen = 0;
static volatile sig_atomic_t g_toggle_sleep = 0;
static volatile sig_atomic_t g_reload_config = 0;

static DaemonConfig g_config;
static SystemTelemetry g_telemetry;
static int g_current_screen = 0;
static bool g_display_sleeping = false;
static bool g_display_dimmed = false;
static time_t g_last_activity_time = 0;
static time_t g_last_screen_switch = 0;
static int g_ipc_sock_fd = -1;

static int get_num_screens(void) {
    int net_cards = (g_telemetry.net_card_count > 0) ? g_telemetry.net_card_count : 1;
    return 2 + net_cards + 1; // 0=CPU, 1=NVMe, 2..(2+net_cards-1)=Nets, last=Clock
}

static void sig_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            g_running = 0;
            break;
        case SIGUSR1:
            g_next_screen = 1;
            break;
        case SIGUSR2:
            g_prev_screen = 1;
            break;
        case SIGHUP:
            g_reload_config = 1;
            break;
    }
}

static void execute_button_action(ButtonAction act) {
    int num_screens = get_num_screens();
    switch (act) {
        case BTN_ACT_NEXT:
            g_current_screen = (g_current_screen + 1) % num_screens;
            g_last_screen_switch = time(NULL);
            break;
        case BTN_ACT_PREV:
            g_current_screen = (g_current_screen - 1 + num_screens) % num_screens;
            g_last_screen_switch = time(NULL);
            break;
        case BTN_ACT_TOGGLE_SLEEP:
            g_display_sleeping = !g_display_sleeping;
            oled_power(!g_display_sleeping);
            if (!g_display_sleeping) {
                oled_set_contrast(g_display_dimmed ? g_config.dim_brightness : g_config.brightness);
            }
            break;
        case BTN_ACT_TOGGLE_DIM:
            g_display_dimmed = !g_display_dimmed;
            oled_set_contrast(g_display_dimmed ? g_config.dim_brightness : g_config.brightness);
            break;
        case BTN_ACT_SHUTDOWN:
            oled_clear();
            oled_draw_string(14, 16, "SYSTEM SHUTDOWN", 1);
            oled_draw_string(24, 34, "Please wait...", 1);
            oled_flush();
            system("systemctl poweroff");
            break;
        case BTN_ACT_REBOOT:
            oled_clear();
            oled_draw_string(20, 16, "SYSTEM REBOOT", 1);
            oled_draw_string(24, 34, "Please wait...", 1);
            oled_flush();
            system("systemctl reboot");
            break;
        case BTN_ACT_NONE:
        default:
            break;
    }
}

static void on_button_event(ButtonEvent ev) {
    g_last_activity_time = time(NULL);

    if (g_display_sleeping) {
        // Any press wakes display up
        g_display_sleeping = false;
        oled_power(true);
        oled_set_contrast(g_display_dimmed ? g_config.dim_brightness : g_config.brightness);
        return;
    }

    switch (ev) {
        case BTN_EVENT_SHORT_CLICK:
            execute_button_action(g_config.btn_action_short);
            break;
        case BTN_EVENT_MEDIUM_HOLD:
            execute_button_action(g_config.btn_action_long);
            break;
        case BTN_EVENT_DOUBLE_CLICK:
            execute_button_action(BTN_ACT_PREV);
            break;
        case BTN_EVENT_LONG_HOLD:
            // Emergency 5s fallback shutdown
            oled_clear();
            oled_draw_string(14, 16, "SYSTEM SHUTDOWN", 1);
            oled_draw_string(24, 34, "Please wait...", 1);
            oled_flush();
            system("systemctl poweroff");
            break;
        default:
            break;
    }
}

static void format_rate(float kbs, char *buf, size_t sz) {
    if (kbs >= 1024.0f) {
        snprintf(buf, sz, "%.1f MB/s", kbs / 1024.0f);
    } else {
        snprintf(buf, sz, "%.0f KB/s", kbs);
    }
}

// ----------------------------------------------------
// KARTE 1: CPU & THERMIK
// ----------------------------------------------------
static void render_screen_cpu_var_a(const SystemTelemetry *t) {
    oled_draw_badge(1, 1, "CPU", 4, 1, true);
    char sub[32];
    snprintf(sub, sizeof(sub), "%.2f GHz", t->cpu_freq_ghz);
    oled_draw_string(44, 4, sub, 1);
    oled_line(1, 16, 126, 16, 1);

    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.1f", t->cpu_temp);
    oled_draw_large_string(4, 21, temp_str, 1);

    // Dynamic x for degree symbol: closer to the hero temperature numbers
    int tw = 4;
    for (const char *p = temp_str; *p; p++) tw += (*p == '.' || *p == ':') ? 7 : 12;
    oled_draw_char(tw + 1, 21, '^', 1);
    oled_draw_med_char(tw + 6, 21, 'C', 1);

    // FAN values shifted up to provide comfortable breathing room from divider line
    oled_draw_badge(82, 19, "FAN", 3, 1, false);
    char fan_buf[16];
    snprintf(fan_buf, sizeof(fan_buf), "%d", t->fan_rpm);
    oled_draw_med_string(82, 30, fan_buf, 1);
    oled_draw_string(82, 42, "RPM", 1);

    oled_line(1, 52, 126, 52, 1);

    for (int i = 0; i < 4; i++) {
        int bx = 3 + i * 31;
        char c_lbl[4];
        snprintf(c_lbl, sizeof(c_lbl), "C%d", i + 1);
        oled_draw_string(bx, 54, c_lbl, 1);
        oled_rect(bx + 13, 55, 14, 7, false, 1);
        int fill_w = (int)((t->core_usage[i] * 12.0f) / 100.0f);
        if (fill_w > 0) oled_rect(bx + 14, 56, fill_w, 5, true, 1);
    }
}

static void render_screen_cpu_var_b(const SystemTelemetry *t) {
    oled_draw_badge(1, 1, "CPU", 4, 1, true);
    char load_buf[16], temp_buf[16];
    snprintf(load_buf, sizeof(load_buf), "%2.0f%%", t->cpu_usage);
    snprintf(temp_buf, sizeof(temp_buf), "%.1f'C", t->cpu_temp);
    oled_draw_med_string(44, 2, load_buf, 1);
    oled_draw_med_string(82, 2, temp_buf, 1);
    oled_line(1, 16, 126, 16, 1);

    oled_draw_sparkline(1, 18, 126, 28, t->temp_history, t->history_count, 35.0f, 75.0f);
    oled_line(1, 48, 126, 48, 1);

    oled_draw_badge(1, 51, "FAN", 3, 1, false);
    char fan_b_str[16];
    snprintf(fan_b_str, sizeof(fan_b_str), "%d RPM", t->fan_rpm);
    oled_draw_string(28, 53, fan_b_str, 1);
    oled_draw_badge(76, 51, "CLK", 3, 1, false);
    char clk_buf[16];
    snprintf(clk_buf, sizeof(clk_buf), "%.1fG", t->cpu_freq_ghz);
    oled_draw_string(102, 53, clk_buf, 1);
}

static void render_screen_cpu_var_c(const SystemTelemetry *t) {
    oled_rect(1, 1, 126, 11, true, 1);
    oled_draw_string(14, 2, "CPU LOAD & THERMALS", 0);

    oled_draw_string(8, 15, "LOAD", 1);
    char load_str[16];
    snprintf(load_str, sizeof(load_str), "%2.0f%%", t->cpu_usage);
    oled_draw_large_string(6, 24, load_str, 1);

    oled_draw_string(72, 15, "TEMP", 1);
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%2.0f", t->cpu_temp);
    oled_draw_large_string(70, 24, temp_str, 1);
    oled_draw_char(98, 24, '^', 1);
    oled_draw_med_char(104, 24, 'C', 1);

    oled_line(63, 14, 63, 49, 1);
    oled_line(1, 50, 126, 50, 1);

    char freq_buf[16];
    snprintf(freq_buf, sizeof(freq_buf), "%.2f GHz", t->cpu_freq_ghz);
    oled_draw_string(4, 53, freq_buf, 1);
    oled_rect(56, 53, 40, 9, false, 1);
    int bar_w = (int)((t->cpu_usage * 38.0f) / 100.0f);
    if (bar_w > 0) oled_rect(57, 54, bar_w, 7, true, 1);
    char lp_buf[8];
    snprintf(lp_buf, sizeof(lp_buf), "%.0f%%", t->cpu_usage);
    oled_draw_string(99, 54, lp_buf, 1);
}

static void render_screen_cpu_graph(const SystemTelemetry *t) {
    switch (g_config.variant_cpu) {
        case 1: render_screen_cpu_var_b(t); break;
        case 2: render_screen_cpu_var_c(t); break;
        default: render_screen_cpu_var_a(t); break;
    }
}

// ----------------------------------------------------
// KARTE 2: SPEICHER (NVMe & RAM)
// ----------------------------------------------------
static void render_screen_nvme_ram_var_a(const SystemTelemetry *t) {
    oled_draw_badge(1, 1, "NVME", 4, 1, true);
    char d_buf[32], dp_buf[16];
    snprintf(d_buf, sizeof(d_buf), "%.0f/%.0fG", t->disk_used_gb, t->disk_total_gb);
    snprintf(dp_buf, sizeof(dp_buf), "%2.0f%%", t->disk_percent);
    oled_draw_string(44, 3, d_buf, 1);
    oled_draw_med_string(98, 2, dp_buf, 1);

    oled_draw_progress_bar(1, 16, 126, 8, t->disk_percent);

    char io_buf[32];
    snprintf(io_buf, sizeof(io_buf), "R: %4.1fM   W: %4.1fM", t->disk_read_mbs, t->disk_write_mbs);
    oled_draw_string(4, 26, io_buf, 1);
    oled_line(1, 34, 126, 34, 1);

    oled_draw_badge(1, 37, "RAM", 4, 1, true);
    char r_buf[32], rp_buf[16];
    snprintf(r_buf, sizeof(r_buf), "%.1f/%.1fG", t->ram_used_mb / 1024.0f, t->ram_total_mb / 1024.0f);
    snprintf(rp_buf, sizeof(rp_buf), "%2.0f%%", t->ram_percent);
    oled_draw_string(40, 39, r_buf, 1);
    oled_draw_med_string(98, 38, rp_buf, 1);

    oled_draw_progress_bar(1, 52, 126, 8, t->ram_percent);
}

static void render_screen_nvme_ram_var_b(const SystemTelemetry *t) {
    oled_rect(1, 1, 61, 13, true, 1);
    oled_draw_med_string(17, 2, "NVME", 0);
    char dp_buf[16];
    snprintf(dp_buf, sizeof(dp_buf), "%2.0f%%", t->disk_percent);
    oled_draw_large_string(8, 17, dp_buf, 1);
    char d_buf[16];
    snprintf(d_buf, sizeof(d_buf), "%.0f/%.0fGB", t->disk_used_gb, t->disk_total_gb);
    oled_draw_string(4, 44, d_buf, 1);
    oled_draw_progress_bar(4, 54, 56, 8, t->disk_percent);

    oled_line(63, 1, 63, 62, 1);

    oled_rect(65, 1, 61, 13, true, 1);
    oled_draw_med_string(84, 2, "RAM", 0);
    char rp_buf[16];
    snprintf(rp_buf, sizeof(rp_buf), "%2.0f%%", t->ram_percent);
    oled_draw_large_string(72, 17, rp_buf, 1);
    char r_buf[16];
    snprintf(r_buf, sizeof(r_buf), "%.1f/%.1fGB", t->ram_used_mb / 1024.0f, t->ram_total_mb / 1024.0f);
    oled_draw_string(66, 44, r_buf, 1);
    oled_draw_progress_bar(67, 54, 57, 8, t->ram_percent);
}

static void render_screen_nvme_ram_var_c(const SystemTelemetry *t) {
    oled_rect(1, 1, 126, 11, true, 1);
    oled_draw_string(16, 2, "STORAGE & MEMORY", 0);

    // Row 1: NVMe
    oled_draw_badge(1, 14, "NVME", 3, 1, false);
    oled_draw_progress_bar(33, 15, 35, 9, t->disk_percent);
    oled_draw_equalizer_bars(71, 15, 9, t->nvme_eq_heights, t->nvme_eq_peaks, 7);
    char dp_buf[16];
    snprintf(dp_buf, sizeof(dp_buf), "%2.0f%%", t->disk_percent);
    oled_draw_med_string(101, 14, dp_buf, 1);
    oled_line(1, 28, 126, 28, 1);

    // Row 2: RAM
    oled_draw_badge(1, 31, "RAM", 3, 1, false);
    oled_draw_progress_bar(33, 32, 35, 9, t->ram_percent);
    oled_draw_equalizer_bars(71, 32, 9, t->ram_eq_heights, t->ram_eq_peaks, 7);
    char rp_buf[16];
    snprintf(rp_buf, sizeof(rp_buf), "%2.0f%%", t->ram_percent);
    oled_draw_med_string(101, 31, rp_buf, 1);

    // Row 3: USB (only if mounted)
    if (t->usb_mounted) {
        oled_line(1, 45, 126, 45, 1);
        oled_draw_badge(1, 48, "USB", 3, 1, false);
        oled_draw_progress_bar(33, 49, 35, 9, t->usb_percent);
        oled_draw_equalizer_bars(71, 49, 9, t->usb_eq_heights, t->usb_eq_peaks, 7);
        char up_buf[16];
        snprintf(up_buf, sizeof(up_buf), "%2.0f%%", t->usb_percent);
        oled_draw_med_string(101, 48, up_buf, 1);
    }
}

static void render_screen_nvme_ram(const SystemTelemetry *t) {
    switch (g_config.variant_mem) {
        case 1: render_screen_nvme_ram_var_b(t); break;
        case 2: render_screen_nvme_ram_var_c(t); break;
        default: render_screen_nvme_ram_var_a(t); break;
    }
}

// ----------------------------------------------------
// KARTE 3: NETZWERK
// ----------------------------------------------------
static void render_screen_net_var_a(const NetCardTelemetry *card, int card_idx, int card_total, int marquee_tick) {
    (void)marquee_tick;
    oled_draw_badge(1, 1, card->is_wifi ? "WLAN" : "ETH", 3, 1, false);
    char name_buf[32];
    snprintf(name_buf, sizeof(name_buf), "%s (1000M)", card->name);
    oled_draw_string(40, 3, name_buf, 1);
    if (card_total > 1) {
        char badge[16];
        snprintf(badge, sizeof(badge), "[%d/%d]", card_idx + 1, card_total);
        oled_draw_string(106, 3, badge, 1);
    }

    oled_rect(1, 13, 126, 17, true, 1);
    int ip_w = (int)strlen(card->ip) * 7 - 1;
    int ip_x = 1 + (126 - ip_w) / 2;
    if (ip_x < 2) ip_x = 2;
    oled_draw_med_string(ip_x, 15, card->ip, 0);

    oled_draw_down_arrow(4, 33, 1);
    char dl_buf[24], ul_buf[24];
    format_rate(card->rx_kbs, dl_buf, sizeof(dl_buf));
    format_rate(card->tx_kbs, ul_buf, sizeof(ul_buf));
    oled_draw_string(12, 33, dl_buf, 1);

    oled_draw_up_arrow(72, 33, 1);
    oled_draw_string(80, 33, ul_buf, 1);
    oled_line(1, 42, 126, 42, 1);

    float max_v = 10.0f;
    for (int i = 0; i < card->history_count; i++) {
        if (card->rx_history[i] > max_v) max_v = card->rx_history[i];
    }
    oled_draw_sparkline(1, 44, 126, 18, card->rx_history, card->history_count, 0.0f, max_v);
}

static void render_screen_net_var_b(const NetCardTelemetry *card, int card_idx, int card_total) {
    (void)card_idx; (void)card_total;
    oled_draw_badge(1, 1, card->is_wifi ? "WLAN" : "ETH", 3, 1, true);
    oled_draw_string(32, 3, card->ip, 1);
    oled_line(1, 16, 126, 16, 1);

    oled_draw_badge(1, 19, "RX", 3, 1, false);
    char dl_buf[24], ul_buf[24];
    format_rate(card->rx_kbs, dl_buf, sizeof(dl_buf));
    format_rate(card->tx_kbs, ul_buf, sizeof(ul_buf));
    oled_draw_string(28, 21, dl_buf, 1);
    oled_draw_sparkline(86, 19, 40, 12, card->rx_history, card->history_count, 0.0f, 1000.0f);
    oled_line(1, 33, 126, 33, 1);

    oled_draw_badge(1, 36, "TX", 3, 1, false);
    oled_draw_string(28, 38, ul_buf, 1);
    oled_draw_sparkline(86, 36, 40, 12, card->rx_history, card->history_count, 0.0f, 1000.0f);
    oled_line(1, 50, 126, 50, 1);

    oled_draw_badge(1, 52, "LINK", 2, 1, false);
    oled_draw_string(34, 53, "1000M Full [UP]", 1);
}

static void render_screen_net_var_c(const NetCardTelemetry *card, int card_idx, int card_total) {
    (void)card_idx; (void)card_total;
    oled_draw_badge(1, 1, card->is_wifi ? "WLAN" : "ETH", 3, 1, false);
    oled_draw_string(44, 3, card->is_wifi ? card->ssid : card->speed_desc, 1);
    oled_line(1, 14, 126, 14, 1);

    oled_draw_down_arrow(6, 23, 1);
    char rate_num[16], rate_unit[16];
    if (card->rx_kbs >= 1024.0f) {
        snprintf(rate_num, sizeof(rate_num), "%.1f", card->rx_kbs / 1024.0f);
        strcpy(rate_unit, "MB/s");
    } else {
        snprintf(rate_num, sizeof(rate_num), "%.0f", card->rx_kbs);
        strcpy(rate_unit, "KB/s");
    }
    oled_draw_large_string(16, 17, rate_num, 1);
    oled_draw_med_string(76, 18, rate_unit, 1);
    oled_draw_string(76, 31, "DOWN", 1);
    oled_line(1, 43, 126, 43, 1);

    oled_draw_up_arrow(6, 46, 1);
    char ul_buf[24];
    format_rate(card->tx_kbs, ul_buf, sizeof(ul_buf));
    oled_draw_string(14, 46, ul_buf, 1);

    oled_draw_badge(1, 53, "IP", 2, 1, false);
    oled_draw_string(24, 54, card->ip, 1);
}

static void render_screen_network_card(const NetCardTelemetry *card, int card_idx, int card_total, int marquee_tick) {
    switch (g_config.variant_net) {
        case 1: render_screen_net_var_b(card, card_idx, card_total); break;
        case 2: render_screen_net_var_c(card, card_idx, card_total); break;
        default: render_screen_net_var_a(card, card_idx, card_total, marquee_tick); break;
    }
}

static void render_screen_network_disconnected(void) {
    oled_draw_badge(2, 2, "NET", 3, 1, false);
    oled_draw_string(30, 3, "Network", 1);

    oled_rect(2, 13, 124, 20, false, 1);
    oled_draw_string(22, 19, "DISCONNECTED", 1);

    oled_draw_string(8, 40, "No active carrier", 1);
    oled_draw_string(8, 52, "Check LAN or Wi-Fi", 1);
}

// ----------------------------------------------------
// KARTE 4: UHR & DATUM
// ----------------------------------------------------
static void render_screen_clock_var_a(const SystemTelemetry *t) {
    char hm_str[8];
    snprintf(hm_str, sizeof(hm_str), "%.5s", t->time_str);
    oled_draw_large_string(26, 1, hm_str, 1);

    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), ":%s", t->time_str + 6);
    oled_draw_med_string(85, 8, sec_buf, 1);

    int date_len = (int)strlen(t->date_str);
    int date_w = date_len * 6 - 1;
    int date_x = (128 - date_w) / 2;
    if (date_x < 1) date_x = 1;
    oled_draw_string(date_x, 28, t->date_str, 1);

    oled_line(1, 38, 126, 38, 1);

    const char *up = (strncmp(t->uptime_str, "Uptime: ", 8) == 0) ? t->uptime_str + 8 : t->uptime_str;
    oled_draw_badge(6, 44, "UPTIME", 4, 2, false);
    oled_draw_med_string(54, 45, up, 1);
}

static void render_screen_clock_var_b(const SystemTelemetry *t) {
    char h_str[4], m_str[4];
    snprintf(h_str, sizeof(h_str), "%.2s", t->time_str);
    snprintf(m_str, sizeof(m_str), "%.2s", t->time_str + 3);

    oled_draw_large_string(10, 4, h_str, 1);
    oled_line(6, 31, 38, 31, 1);
    oled_draw_large_string(10, 35, m_str, 1);

    oled_line(44, 1, 44, 62, 1);

    oled_draw_badge(48, 2, "DATUM", 3, 1, false);
    const char *comma = strchr(t->date_str, ',');
    if (comma) {
        char day_str[16];
        int len = comma - t->date_str;
        if (len >= (int)sizeof(day_str)) len = sizeof(day_str) - 1;
        strncpy(day_str, t->date_str, len);
        day_str[len] = '\0';
        oled_draw_string(48, 16, comma + 2, 1);
        oled_draw_string(48, 25, day_str, 1);
    } else {
        oled_draw_string(48, 18, t->date_str, 1);
    }

    oled_line(48, 35, 126, 35, 1);
    oled_draw_badge(48, 38, "UPTIME", 3, 1, false);
    const char *up = (strncmp(t->uptime_str, "Uptime: ", 8) == 0) ? t->uptime_str + 8 : t->uptime_str;
    oled_draw_string(48, 52, up, 1);
}

static void render_screen_clock_var_c(const SystemTelemetry *t) {
    oled_rect(1, 1, 126, 11, true, 1);
    oled_draw_string(2, 2, "ARGON ONE V5 [ONLINE]", 0);

    char hm_str[8];
    snprintf(hm_str, sizeof(hm_str), "%.5s", t->time_str);
    oled_draw_large_string(14, 16, hm_str, 1);

    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), ":%s", t->time_str + 6);
    oled_draw_med_string(73, 23, sec_buf, 1);
    oled_draw_string(98, 26, "CET", 1);

    oled_line(1, 43, 126, 43, 1);

    oled_draw_string(4, 46, t->date_str, 1);
    const char *up = (strncmp(t->uptime_str, "Uptime: ", 8) == 0) ? t->uptime_str + 8 : t->uptime_str;
    char up_buf[32];
    snprintf(up_buf, sizeof(up_buf), "UPTIME: %s", up);
    oled_draw_string(4, 54, up_buf, 1);
}

static void render_screen_big_clock(const SystemTelemetry *t) {
    switch (g_config.variant_clock) {
        case 1: render_screen_clock_var_b(t); break;
        case 2: render_screen_clock_var_c(t); break;
        default: render_screen_clock_var_a(t); break;
    }
}

static void render_alert_screen(const SystemTelemetry *t) {
    oled_rect(0, 0, 128, 64, false, 1);
    oled_rect(2, 2, 124, 60, false, 1);

    oled_draw_string(12, 8, "! OVERHEAT ALERT !", 1);

    char buf[32];
    snprintf(buf, sizeof(buf), "%4.1f°C", t->cpu_temp);
    oled_draw_large_string(32, 22, buf, 1);

    oled_draw_string(16, 48, "Check Fan & Airflow", 1);
}

static int setup_ipc_socket(void) {
    unlink(SOCKET_PATH);
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    chmod(SOCKET_PATH, 0666);
    return fd;
}

static void handle_ipc_messages(void) {
    if (g_ipc_sock_fd < 0) return;

    char buf[128];
    ssize_t n = recv(g_ipc_sock_fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        g_last_activity_time = time(NULL);

        int num_screens = get_num_screens();
        if (strcasecmp(buf, "next") == 0) {
            g_current_screen = (g_current_screen + 1) % num_screens;
            g_last_screen_switch = time(NULL);
        } else if (strcasecmp(buf, "prev") == 0) {
            g_current_screen = (g_current_screen - 1 + num_screens) % num_screens;
            g_last_screen_switch = time(NULL);
        } else if (strcasecmp(buf, "toggle") == 0) {
            g_display_sleeping = !g_display_sleeping;
            oled_power(!g_display_sleeping);
        } else if (strcasecmp(buf, "on") == 0) {
            g_display_sleeping = false;
            oled_power(true);
        } else if (strcasecmp(buf, "off") == 0) {
            g_display_sleeping = true;
            oled_power(false);
        } else if (strcasecmp(buf, "dim") == 0) {
            g_display_dimmed = !g_display_dimmed;
            oled_set_contrast(g_display_dimmed ? g_config.dim_brightness : g_config.brightness);
        } else if (strncmp(buf, "brightness:", 11) == 0) {
            int b = atoi(buf + 11);
            if (b >= 0 && b <= 255) {
                g_config.brightness = (uint8_t)b;
                if (!g_display_dimmed) oled_set_contrast(g_config.brightness);
                config_save_file("/etc/argon-v5-oled.conf", &g_config);
            }
        } else if (strncmp(buf, "dim_brightness:", 15) == 0) {
            int b = atoi(buf + 15);
            if (b >= 0 && b <= 255) {
                g_config.dim_brightness = (uint8_t)b;
                if (g_display_dimmed) oled_set_contrast(g_config.dim_brightness);
                config_save_file("/etc/argon-v5-oled.conf", &g_config);
            }
        } else if (strncmp(buf, "screen:", 7) == 0) {
            int s = atoi(buf + 7);
            if (s >= 0 && s < num_screens) {
                g_current_screen = s;
                g_last_screen_switch = time(NULL);
            }
        } else if (strncmp(buf, "set:", 4) == 0) {
            char key[64], val[64];
            if (sscanf(buf + 4, "%63[^:]:%63s", key, val) == 2) {
                if (strcasecmp(key, "screen_duration") == 0) {
                    g_config.screen_duration_secs = atoi(val);
                } else if (strcasecmp(key, "screensaver_timeout") == 0) {
                    g_config.screensaver_timeout_secs = atoi(val);
                } else if (strcasecmp(key, "brightness") == 0) {
                    g_config.brightness = (uint8_t)atoi(val);
                    if (!g_display_dimmed) oled_set_contrast(g_config.brightness);
                } else if (strcasecmp(key, "dim_brightness") == 0) {
                    g_config.dim_brightness = (uint8_t)atoi(val);
                    if (g_display_dimmed) oled_set_contrast(g_config.dim_brightness);
                } else if (strcasecmp(key, "btn_action_short") == 0) {
                    g_config.btn_action_short = parse_button_action(val);
                } else if (strcasecmp(key, "btn_action_long") == 0) {
                    g_config.btn_action_long = parse_button_action(val);
                } else if (strcasecmp(key, "enable_pixel_shift") == 0) {
                    g_config.enable_pixel_shift = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0);
                }
                config_save_file("/etc/argon-v5-oled.conf", &g_config);
            }
        } else if (strcasecmp(buf, "reload") == 0) {
            config_load_file("/etc/argon-v5-oled.conf", &g_config);
            oled_set_contrast(g_display_dimmed ? g_config.dim_brightness : g_config.brightness);
        } else if (strncmp(buf, "variant:", 8) == 0) {
            char target[32], val[32];
            if (sscanf(buf + 8, "%31[^:]:%31s", target, val) == 2) {
                int var = 0;
                if (strcasecmp(val, "b") == 0 || strcmp(val, "1") == 0) var = 1;
                else if (strcasecmp(val, "c") == 0 || strcmp(val, "2") == 0) var = 2;

                if (strcasecmp(target, "cpu") == 0) g_config.variant_cpu = var;
                else if (strcasecmp(target, "mem") == 0 || strcasecmp(target, "ram") == 0) g_config.variant_mem = var;
                else if (strcasecmp(target, "net") == 0) g_config.variant_net = var;
                else if (strcasecmp(target, "clock") == 0) g_config.variant_clock = var;
                else if (strcasecmp(target, "all") == 0) {
                    g_config.variant_cpu = var;
                    g_config.variant_mem = var;
                    g_config.variant_net = var;
                    g_config.variant_clock = var;
                }
                g_last_screen_switch = time(NULL);
                config_save_file("/etc/argon-v5-oled.conf", &g_config);
            }
        }
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGUSR2, sig_handler);
    signal(SIGHUP, sig_handler);

    config_load_file("/etc/argon-v5-oled.conf", &g_config);

    if (oled_init(g_config.i2c_bus, g_config.i2c_address) < 0) {
        fprintf(stderr, "Fatal: failed to initialize SSD1306 on %s addr 0x%02x\n",
                g_config.i2c_bus, g_config.i2c_address);
        return 1;
    }
    oled_set_contrast(g_config.brightness);

    telemetry_init();

    if (button_init(g_config.gpio_chip, g_config.gpio_pin, on_button_event) < 0) {
        fprintf(stderr, "Warning: failed to initialize button on %s pin %u\n",
                g_config.gpio_chip, g_config.gpio_pin);
    }

    g_ipc_sock_fd = setup_ipc_socket();

    g_last_activity_time = time(NULL);
    g_last_screen_switch = time(NULL);
    time_t last_pixel_shift = time(NULL);
    int shift_step = 0;
    int marquee_tick = 0;

    printf("argon-v5-oled daemon started successfully!\n");

    while (g_running) {
        time_t now = time(NULL);

        if (g_reload_config) {
            g_reload_config = 0;
            config_load_file("/etc/argon-v5-oled.conf", &g_config);
            oled_set_contrast(g_config.brightness);
        }

        telemetry_update(&g_telemetry);

        int num_screens = get_num_screens();
        if (g_current_screen >= num_screens) {
            g_current_screen = num_screens - 1;
        }

        if (g_next_screen) {
            g_next_screen = 0;
            g_current_screen = (g_current_screen + 1) % num_screens;
            g_last_activity_time = now;
            g_last_screen_switch = now;
        }
        if (g_prev_screen) {
            g_prev_screen = 0;
            g_current_screen = (g_current_screen - 1 + num_screens) % num_screens;
            g_last_activity_time = now;
            g_last_screen_switch = now;
        }

        handle_ipc_messages();

        // Check for overheat alert condition
        bool is_overheat = (g_telemetry.cpu_temp >= g_config.temp_alert_threshold);
        if (is_overheat && g_display_sleeping) {
            g_display_sleeping = false;
            oled_power(true);
            oled_set_contrast(g_config.brightness);
        }

        // Check screensaver timeout
        if (!is_overheat && g_config.screensaver_timeout_secs > 0) {
            if (!g_display_sleeping && (now - g_last_activity_time >= g_config.screensaver_timeout_secs)) {
                g_display_sleeping = true;
                oled_power(false);
            }
        }

        // Auto screen cycling (if enabled and display awake)
        if (!g_display_sleeping && !is_overheat && g_config.screen_duration_secs > 0) {
            if (now - g_last_screen_switch >= g_config.screen_duration_secs) {
                g_current_screen = (g_current_screen + 1) % num_screens;
                g_last_screen_switch = now;
            }
        }

        // Pixel-shift burn-in protection every 120s
        if (g_config.enable_pixel_shift && (now - last_pixel_shift >= 120)) {
            static const int shifts[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
            shift_step = (shift_step + 1) % 4;
            oled_set_pixel_shift(shifts[shift_step][0], shifts[shift_step][1]);
            last_pixel_shift = now;
        }

        // Render frame if display awake
        if (!g_display_sleeping) {
            oled_clear();

            if (is_overheat) {
                render_alert_screen(&g_telemetry);
            } else {
                int net_cards = (g_telemetry.net_card_count > 0) ? g_telemetry.net_card_count : 1;
                int clock_screen_idx = 2 + net_cards;

                if (g_current_screen == 0) {
                    render_screen_cpu_graph(&g_telemetry);
                } else if (g_current_screen == 1) {
                    render_screen_nvme_ram(&g_telemetry);
                } else if (g_current_screen >= 2 && g_current_screen < clock_screen_idx) {
                    if (g_telemetry.net_card_count == 0) {
                        render_screen_network_disconnected();
                    } else {
                        int card_idx = g_current_screen - 2;
                        render_screen_network_card(&g_telemetry.net_cards[card_idx], card_idx, g_telemetry.net_card_count, marquee_tick);
                    }
                } else {
                    render_screen_big_clock(&g_telemetry);
                }
            }

            oled_flush();
        }

        marquee_tick += 3;
        if (g_ipc_sock_fd >= 0) {
            struct pollfd pfd;
            pfd.fd = g_ipc_sock_fd;
            pfd.events = POLLIN;
            poll(&pfd, 1, 1000);
        } else {
            sleep(1);
        }
    }

    // Clean exit
    if (g_ipc_sock_fd >= 0) {
        close(g_ipc_sock_fd);
        unlink(SOCKET_PATH);
    }
    button_stop();
    oled_clear();
    oled_flush();
    oled_close();

    printf("argon-v5-oled daemon terminated cleanly.\n");
    return 0;
}
