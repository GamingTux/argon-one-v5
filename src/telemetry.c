#include "telemetry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// #include <net/if.h>

static unsigned long long prev_total = 0, prev_idle = 0;
static unsigned long long prev_core_total[4] = {0}, prev_core_idle[4] = {0};
static unsigned long long prev_nvme_read_sectors = 0, prev_nvme_write_sectors = 0;
static struct timespec prev_time;

// Sensor & hardware discovery paths (dynamically resolved)
static char s_cpu_temp_path[128] = "/sys/class/thermal/thermal_zone0/temp";
static char s_nvme_temp_path[128] = "";
static char s_rp1_temp_path[128] = "";
static char s_fan_rpm_path[128] = "";
static char s_undervolt_path[128] = "";
static char s_nvme_stat_path[128] = "/sys/block/nvme0n1/stat";
static char s_gpu_stats_path[128] = "/sys/class/drm/card0/device/gpu_stats";

static unsigned long long prev_gpu_ts = 0;
static unsigned long long prev_gpu_runtime = 0;

// Synthesizer Equalizer Peak Tracking
static int prev_nvme_peaks[7] = {1, 1, 1, 1, 1, 1, 1};
static int prev_nvme_peak_hold[7] = {0};

static int prev_ram_peaks[7] = {1, 1, 1, 1, 1, 1, 1};
static int prev_ram_peak_hold[7] = {0};
static unsigned long long prev_pgfault = 0;

static int prev_usb_peaks[7] = {1, 1, 1, 1, 1, 1, 1};
static int prev_usb_peak_hold[7] = {0};
static unsigned long long prev_usb_read_sectors = 0, prev_usb_write_sectors = 0;

static void update_eq_channel(float activity_val, float max_scale, int base_offset,
                              int out_heights[7], int out_peaks[7],
                              int prev_peaks[7], int prev_peak_hold[7]) {
    (void)max_scale;
    if (activity_val < 0.01f && base_offset <= 1) {
        for (int i = 0; i < 7; i++) {
            out_heights[i] = 1;
            if (prev_peak_hold[i] > 0) {
                prev_peak_hold[i]--;
            } else if (prev_peaks[i] > 1) {
                prev_peaks[i]--;
            }
            out_peaks[i] = prev_peaks[i];
        }
        return;
    }

    float log_val = 0.0f;
    if (activity_val > 0.01f) {
        log_val = 1.0f + 2.5f * log10f(activity_val + 1.0f);
    }

    static const float col_weights[7] = { 0.85f, 1.05f, 1.25f, 1.15f, 1.00f, 0.90f, 0.75f };

    for (int i = 0; i < 7; i++) {
        int h = base_offset + (int)(log_val * col_weights[i]);
        if (h < 1) h = 1;
        if (h > 9) h = 9;
        out_heights[i] = h;

        if (h >= prev_peaks[i]) {
            prev_peaks[i] = h;
            prev_peak_hold[i] = 2; // hold peak for 2 seconds
        } else {
            if (prev_peak_hold[i] > 0) {
                prev_peak_hold[i]--;
            } else if (prev_peaks[i] > 1) {
                prev_peaks[i]--;
            }
        }
        out_peaks[i] = prev_peaks[i];
    }
}

static void discover_hardware_sensors(void) {
    // Scan hwmon 0 to 7
    for (int i = 0; i < 8; i++) {
        char p[128];
        snprintf(p, sizeof(p), "/sys/class/hwmon/hwmon%d/name", i);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        char name[64];
        if (fgets(name, sizeof(name), f)) {
            char *nl = strchr(name, '\n');
            if (nl) *nl = '\0';
            if (strcmp(name, "cpu_thermal") == 0) {
                snprintf(s_cpu_temp_path, sizeof(s_cpu_temp_path), "/sys/class/hwmon/hwmon%d/temp1_input", i);
            } else if (strcmp(name, "nvme") == 0) {
                snprintf(s_nvme_temp_path, sizeof(s_nvme_temp_path), "/sys/class/hwmon/hwmon%d/temp1_input", i);
            } else if (strcmp(name, "rp1_adc") == 0) {
                snprintf(s_rp1_temp_path, sizeof(s_rp1_temp_path), "/sys/class/hwmon/hwmon%d/temp1_input", i);
            } else if (strcmp(name, "pwmfan") == 0) {
                snprintf(s_fan_rpm_path, sizeof(s_fan_rpm_path), "/sys/class/hwmon/hwmon%d/fan1_input", i);
            } else if (strcmp(name, "rpi_volt") == 0) {
                snprintf(s_undervolt_path, sizeof(s_undervolt_path), "/sys/class/hwmon/hwmon%d/in0_lcrit_alarm", i);
            }
        }
        fclose(f);
    }

    // Dynamic NVMe block detection (find first nvme*n1)
    for (int i = 0; i < 4; i++) {
        char bpath[128];
        snprintf(bpath, sizeof(bpath), "/sys/block/nvme%dn1/stat", i);
        if (access(bpath, F_OK) == 0) {
            snprintf(s_nvme_stat_path, sizeof(s_nvme_stat_path), "%s", bpath);
            break;
        }
    }
}

void telemetry_init(void) {
    clock_gettime(CLOCK_MONOTONIC, &prev_time);
    discover_hardware_sensors();
}

static void read_cpu_temp_and_freq(SystemTelemetry *t) {
    t->cpu_temp = 0.0f;
    t->cpu_freq_ghz = 0.0f;

    FILE *f = fopen(s_cpu_temp_path, "r");
    if (!f) f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        long mdeg = 0;
        if (fscanf(f, "%ld", &mdeg) == 1) {
            t->cpu_temp = mdeg / 1000.0f;
        }
        fclose(f);
    }

    f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (f) {
        long khz = 0;
        if (fscanf(f, "%ld", &khz) == 1) {
            t->cpu_freq_ghz = khz / 1000000.0f;
        }
        fclose(f);
    }
}

static void read_fan_rpm(SystemTelemetry *t) {
    t->fan_rpm = 0;
    if (s_fan_rpm_path[0]) {
        FILE *f = fopen(s_fan_rpm_path, "r");
        if (f) {
            int rpm = 0;
            if (fscanf(f, "%d", &rpm) == 1) t->fan_rpm = rpm;
            fclose(f);
            return;
        }
    }
}

static void read_gpu_telemetry(SystemTelemetry *t) {
    t->has_gpu = false;
    t->gpu_usage = 0.0f;
    t->gpu_freq_mhz = 0;

    if (access(s_gpu_stats_path, F_OK) != 0) return;
    t->has_gpu = true;

    FILE *f = fopen(s_gpu_stats_path, "r");
    if (f) {
        char line[256];
        unsigned long long cur_ts = 0;
        unsigned long long active_ns = 0;
        while (fgets(line, sizeof(line), f)) {
            char q[32];
            unsigned long long ts, jobs, rt;
            if (sscanf(line, "%31s %llu %llu %llu", q, &ts, &jobs, &rt) == 4) {
                cur_ts = ts;
                if (strcmp(q, "render") == 0 || strcmp(q, "bin") == 0 || strcmp(q, "tfu") == 0) {
                    active_ns += rt;
                }
            }
        }
        fclose(f);

        if (prev_gpu_ts > 0 && cur_ts > prev_gpu_ts) {
            unsigned long long d_ts = cur_ts - prev_gpu_ts;
            unsigned long long d_act = (active_ns >= prev_gpu_runtime) ? (active_ns - prev_gpu_runtime) : 0;
            float load = (100.0f * (float)d_act) / (float)d_ts;
            if (load > 100.0f) load = 100.0f;
            t->gpu_usage = load;
        }
        prev_gpu_ts = cur_ts;
        prev_gpu_runtime = active_ns;
    }

    FILE *f_clk = fopen("/sys/kernel/debug/dri/1002000000.v3d/measure_clock", "r");
    if (f_clk) {
        char buf[128];
        if (fgets(buf, sizeof(buf), f_clk)) {
            char *mhz = strstr(buf, "(");
            if (mhz) {
                float f_mhz = 0.0f;
                if (sscanf(mhz + 1, "%f", &f_mhz) == 1) {
                    t->gpu_freq_mhz = (int)f_mhz;
                }
            }
        }
        fclose(f_clk);
    }
}

static void read_advanced_sensors(SystemTelemetry *t) {
    t->nvme_temp = 0.0f;
    t->rp1_temp = 0.0f;
    t->undervoltage_alarm = false;
    t->in_5v_volt = 5.0f;

    if (s_nvme_temp_path[0]) {
        FILE *f = fopen(s_nvme_temp_path, "r");
        if (f) {
            long mdeg = 0;
            if (fscanf(f, "%ld", &mdeg) == 1) t->nvme_temp = mdeg / 1000.0f;
            fclose(f);
        }
    }
    if (s_rp1_temp_path[0]) {
        FILE *f = fopen(s_rp1_temp_path, "r");
        if (f) {
            long mdeg = 0;
            if (fscanf(f, "%ld", &mdeg) == 1) t->rp1_temp = mdeg / 1000.0f;
            fclose(f);
        }
    }
    if (s_undervolt_path[0]) {
        FILE *f = fopen(s_undervolt_path, "r");
        if (f) {
            int alarm = 0;
            if (fscanf(f, "%d", &alarm) == 1) t->undervoltage_alarm = (alarm != 0);
            fclose(f);
        }
    }
}

static void read_raid_and_sd(SystemTelemetry *t) {
    t->raid_active = false;
    t->raid_degraded = false;
    t->raid_level[0] = '\0';
    t->raid_health[0] = '\0';
    t->sd_present = false;
    t->sd_size_gb = 0.0f;

    // Check /proc/mdstat
    FILE *f_md = fopen("/proc/mdstat", "r");
    if (f_md) {
        char line[256];
        while (fgets(line, sizeof(line), f_md)) {
            char md_name[32], state[32], level[32];
            if (sscanf(line, "%31s : %31s %31s", md_name, state, level) >= 3) {
                if (strncmp(md_name, "md", 2) == 0) {
                    t->raid_active = true;
                    snprintf(t->raid_level, sizeof(t->raid_level), "%s", level);
                    snprintf(t->raid_health, sizeof(t->raid_health), "OK");
                }
            }
            if (t->raid_active && strchr(line, '[') && strchr(line, ']')) {
                if (strstr(line, "[U_]") || strstr(line, "[_U]") || strstr(line, "_")) {
                    t->raid_degraded = true;
                    snprintf(t->raid_health, sizeof(t->raid_health), "DEGRADED");
                }
            }
        }
        fclose(f_md);
    }

    // MicroSD Check
    FILE *f_sd = fopen("/sys/block/mmcblk0/size", "r");
    if (f_sd) {
        unsigned long long sec = 0;
        if (fscanf(f_sd, "%llu", &sec) == 1 && sec > 0) {
            t->sd_present = true;
            t->sd_size_gb = (float)((sec * 512.0) / (1024.0 * 1024.0 * 1024.0));
        }
        fclose(f_sd);
    }
}

static void read_cpu_usage(SystemTelemetry *t) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return;

    char line[256];
    int core_idx = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            if (sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu",
                       &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
                unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
                unsigned long long d_total = total - prev_total;
                unsigned long long d_idle = idle - prev_idle;

                if (d_total > 0 && prev_total > 0) {
                    t->cpu_usage = 100.0f * (1.0f - ((float)d_idle / (float)d_total));
                }
                prev_total = total;
                prev_idle = idle;
            }
        } else if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '3' && core_idx < 4) {
            int c = line[3] - '0';
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            if (sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu",
                       &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
                unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
                unsigned long long d_total = total - prev_core_total[c];
                unsigned long long d_idle = idle - prev_core_idle[c];

                if (d_total > 0 && prev_core_total[c] > 0) {
                    t->core_usage[c] = 100.0f * (1.0f - ((float)d_idle / (float)d_total));
                }
                prev_core_total[c] = total;
                prev_core_idle[c] = idle;
            }
            core_idx++;
        }
    }
    fclose(f);
}

static void read_mem_usage(SystemTelemetry *t) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[128];
        long total_kb = 0, avail_kb = 0;

        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line + 9, "%ld", &total_kb);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                sscanf(line + 13, "%ld", &avail_kb);
            }
        }
        fclose(f);

        if (total_kb > 0) {
            t->ram_total_mb = total_kb / 1024;
            t->ram_used_mb = (total_kb - avail_kb) / 1024;
            t->ram_percent = (100.0f * (float)t->ram_used_mb) / (float)t->ram_total_mb;
        }
    }

    // Read memory page allocations & fault activity
    FILE *f_vm = fopen("/proc/vmstat", "r");
    unsigned long long cur_pgfault = 0;
    if (f_vm) {
        char vm_line[128];
        while (fgets(vm_line, sizeof(vm_line), f_vm)) {
            if (strncmp(vm_line, "pgfault ", 8) == 0) {
                sscanf(vm_line + 8, "%llu", &cur_pgfault);
                break;
            }
        }
        fclose(f_vm);
    }
    float pg_rate = 0.0f;
    if (prev_pgfault > 0 && cur_pgfault >= prev_pgfault) {
        pg_rate = (float)(cur_pgfault - prev_pgfault) / 1000.0f;
    }
    prev_pgfault = cur_pgfault;

    int ram_base = 1 + (int)(t->ram_percent / 25.0f);
    if (ram_base > 4) ram_base = 4;
    update_eq_channel(pg_rate, 100.0f, ram_base, t->ram_eq_heights, t->ram_eq_peaks, prev_ram_peaks, prev_ram_peak_hold);
}

static void read_disk_usage_and_io(SystemTelemetry *t, double dt) {
    struct statvfs sv;
    if (statvfs("/", &sv) == 0) {
        double total_bytes = (double)sv.f_blocks * sv.f_frsize;
        double free_bytes = (double)sv.f_bavail * sv.f_frsize;
        double used_bytes = total_bytes - free_bytes;

        t->disk_total_gb = total_bytes / (1024.0 * 1024.0 * 1024.0);
        t->disk_used_gb = used_bytes / (1024.0 * 1024.0 * 1024.0);
        t->disk_percent = (total_bytes > 0) ? (100.0f * (used_bytes / total_bytes)) : 0.0f;
    }

    t->disk_read_mbs = 0.0f;
    t->disk_write_mbs = 0.0f;
    t->usb_read_mbs = 0.0f;
    t->usb_write_mbs = 0.0f;

    // Scan for mounted USB mass storage device (/dev/sd*)
    t->usb_mounted = false;
    t->usb_used_gb = 0.0f;
    t->usb_total_gb = 0.0f;
    t->usb_percent = 0.0f;
    t->usb_name[0] = '\0';
    char usb_dev_name[64] = {0};

    FILE *f_mounts = fopen("/proc/mounts", "r");
    if (f_mounts) {
        char mline[512];
        while (fgets(mline, sizeof(mline), f_mounts)) {
            char dev[64], mnt[256], fstype[32];
            if (sscanf(mline, "%63s %255s %31s", dev, mnt, fstype) >= 2) {
                if (strncmp(dev, "/dev/sd", 7) == 0 && access(dev, F_OK) == 0) {
                    struct statvfs usv;
                    if (statvfs(mnt, &usv) == 0) {
                        double utotal = (double)usv.f_blocks * usv.f_frsize;
                        double ufree = (double)usv.f_bavail * usv.f_frsize;
                        double uused = utotal - ufree;
                        if (utotal > 0) {
                            t->usb_mounted = true;
                            t->usb_total_gb = (float)(utotal / (1024.0 * 1024.0 * 1024.0));
                            t->usb_used_gb = (float)(uused / (1024.0 * 1024.0 * 1024.0));
                            t->usb_percent = (float)(100.0f * (uused / utotal));
                            strncpy(usb_dev_name, dev + 5, sizeof(usb_dev_name) - 1);
                            const char *slash = strrchr(mnt, '/');
                            if (slash && *(slash + 1)) {
                                snprintf(t->usb_name, sizeof(t->usb_name), "%s", slash + 1);
                            } else {
                                snprintf(t->usb_name, sizeof(t->usb_name), "%s", dev + 5);
                            }
                            break;
                        }
                    }
                }
            }
        }
        fclose(f_mounts);
    }

    // Read NVMe I/O stats
    FILE *f = fopen(s_nvme_stat_path, "r");
    if (f) {
        unsigned long long read_sectors = 0, write_sectors = 0;
        unsigned long long dummy;
        if (fscanf(f, "%llu %llu %llu %llu %llu %llu %llu",
                   &dummy, &dummy, &read_sectors, &dummy, &dummy, &dummy, &write_sectors) >= 7) {
            if (prev_nvme_read_sectors > 0 && dt > 0.001) {
                unsigned long long d_read = (read_sectors >= prev_nvme_read_sectors) ? (read_sectors - prev_nvme_read_sectors) : 0;
                unsigned long long d_write = (write_sectors >= prev_nvme_write_sectors) ? (write_sectors - prev_nvme_write_sectors) : 0;
                t->disk_read_mbs = (float)((d_read * 512.0) / (1024.0 * 1024.0 * dt));
                t->disk_write_mbs = (float)((d_write * 512.0) / (1024.0 * 1024.0 * dt));
            }
            prev_nvme_read_sectors = read_sectors;
            prev_nvme_write_sectors = write_sectors;
        }
        fclose(f);
    }

    // Read USB I/O stats if mounted
    if (t->usb_mounted && usb_dev_name[0]) {
        char stat_path[128];
        snprintf(stat_path, sizeof(stat_path), "/sys/class/block/%s/stat", usb_dev_name);
        FILE *f_usb = fopen(stat_path, "r");
        if (f_usb) {
            unsigned long long u_read = 0, u_write = 0, dummy;
            if (fscanf(f_usb, "%llu %llu %llu %llu %llu %llu %llu",
                       &dummy, &dummy, &u_read, &dummy, &dummy, &dummy, &u_write) >= 7) {
                if (prev_usb_read_sectors > 0 && dt > 0.001) {
                    unsigned long long d_r = (u_read >= prev_usb_read_sectors) ? (u_read - prev_usb_read_sectors) : 0;
                    unsigned long long d_w = (u_write >= prev_usb_write_sectors) ? (u_write - prev_usb_write_sectors) : 0;
                    t->usb_read_mbs = (float)((d_r * 512.0) / (1024.0 * 1024.0 * dt));
                    t->usb_write_mbs = (float)((d_w * 512.0) / (1024.0 * 1024.0 * dt));
                }
                prev_usb_read_sectors = u_read;
                prev_usb_write_sectors = u_write;
            }
            fclose(f_usb);
        }
    } else {
        prev_usb_read_sectors = 0;
        prev_usb_write_sectors = 0;
    }

    // Update NVMe Equalizer
    float nvme_mbs = t->disk_read_mbs + t->disk_write_mbs;
    update_eq_channel(nvme_mbs, 100.0f, 0, t->nvme_eq_heights, t->nvme_eq_peaks, prev_nvme_peaks, prev_nvme_peak_hold);

    // Update USB Equalizer
    if (t->usb_mounted) {
        float usb_mbs = t->usb_read_mbs + t->usb_write_mbs;
        update_eq_channel(usb_mbs, 50.0f, 0, t->usb_eq_heights, t->usb_eq_peaks, prev_usb_peaks, prev_usb_peak_hold);
    } else {
        for (int i = 0; i < 7; i++) {
            t->usb_eq_heights[i] = 1;
            t->usb_eq_peaks[i] = 1;
        }
    }
}

static void get_wifi_ssid(const char *ifname, char *ssid, int max_len) {
    ssid[0] = '\0';
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    struct iwreq wrq;
    memset(&wrq, 0, sizeof(wrq));
    strncpy(wrq.ifr_name, ifname, IFNAMSIZ);

    char essid[IW_ESSID_MAX_SIZE + 1] = {0};
    wrq.u.essid.pointer = (caddr_t)essid;
    wrq.u.essid.length = IW_ESSID_MAX_SIZE;
    wrq.u.essid.flags = 0;

    if (ioctl(sock, SIOCGIWESSID, &wrq) == 0) {
        strncpy(ssid, essid, max_len - 1);
        ssid[max_len - 1] = '\0';
    }
    close(sock);
}

// Persistent tracking across calls so rx_history and prev bytes persist
static NetCardTelemetry s_saved_cards[MAX_NET_CARDS];
static int s_saved_card_count = 0;

static void read_network(SystemTelemetry *t, double dt) {
    t->net_card_count = 0;

    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) < 0) return;

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL && t->net_card_count < MAX_NET_CARDS; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;

        const char *name = ifa->ifa_name;
        // Skip virtual bridge/docker interfaces
        if (strncmp(name, "veth", 4) == 0 || strncmp(name, "docker", 6) == 0 || strncmp(name, "br-", 3) == 0) continue;

        // Check carrier (must have physical link or associated wifi)
        char path[128];
        snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", name);
        FILE *fc = fopen(path, "r");
        int carrier = 0;
        if (fc) {
            if (fscanf(fc, "%d", &carrier) != 1) carrier = 0;
            fclose(fc);
        }
        if (!carrier) continue; // Skip disconnected interfaces!

        NetCardTelemetry *card = &t->net_cards[t->net_card_count];
        memset(card, 0, sizeof(*card));
        strncpy(card->name, name, sizeof(card->name) - 1);

        // IP address
        void *sin_addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, sin_addr, card->ip, sizeof(card->ip));

        // Check if Wi-Fi
        snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", name);
        if (access(path, F_OK) == 0) {
            card->is_wifi = true;
            get_wifi_ssid(name, card->ssid, sizeof(card->ssid));
        } else {
            snprintf(path, sizeof(path), "/sys/class/net/%s/phy80211", name);
            if (access(path, F_OK) == 0) {
                card->is_wifi = true;
                get_wifi_ssid(name, card->ssid, sizeof(card->ssid));
            }
        }

        // Check if USB
        snprintf(path, sizeof(path), "/sys/class/net/%s/device", name);
        char link_target[256];
        ssize_t len = readlink(path, link_target, sizeof(link_target) - 1);
        if (len > 0) {
            link_target[len] = '\0';
            if (strstr(link_target, "/usb") != NULL) {
                card->is_usb = true;
            }
        }

        // Link speed
        snprintf(path, sizeof(path), "/sys/class/net/%s/speed", name);
        FILE *fs = fopen(path, "r");
        int speed = -1;
        if (fs) {
            if (fscanf(fs, "%d", &speed) != 1) speed = -1;
            fclose(fs);
        }
        if (speed >= 1000) {
            snprintf(card->speed_desc, sizeof(card->speed_desc), "%dM Full", speed);
        } else if (speed > 0) {
            snprintf(card->speed_desc, sizeof(card->speed_desc), "%dM", speed);
        } else {
            snprintf(card->speed_desc, sizeof(card->speed_desc), "Connected");
        }

        // Read rx/tx bytes
        unsigned long long rx_bytes = 0, tx_bytes = 0;
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", name);
        FILE *fr = fopen(path, "r");
        if (fr) {
            if (fscanf(fr, "%llu", &rx_bytes) != 1) rx_bytes = 0;
            fclose(fr);
        }
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", name);
        FILE *ft = fopen(path, "r");
        if (ft) {
            if (fscanf(ft, "%llu", &tx_bytes) != 1) tx_bytes = 0;
            fclose(ft);
        }

        // Find matching saved card to preserve history
        NetCardTelemetry *prev = NULL;
        for (int sc = 0; sc < s_saved_card_count; sc++) {
            if (strcmp(s_saved_cards[sc].name, name) == 0) {
                prev = &s_saved_cards[sc];
                break;
            }
        }

        if (prev && prev->prev_rx_bytes > 0 && dt > 0.001) {
            unsigned long long d_rx = (rx_bytes >= prev->prev_rx_bytes) ? (rx_bytes - prev->prev_rx_bytes) : 0;
            unsigned long long d_tx = (tx_bytes >= prev->prev_tx_bytes) ? (tx_bytes - prev->prev_tx_bytes) : 0;
            card->rx_kbs = (float)(d_rx / (1024.0 * dt));
            card->tx_kbs = (float)(d_tx / (1024.0 * dt));

            // Copy history
            memcpy(card->rx_history, prev->rx_history, sizeof(card->rx_history));
            card->history_count = prev->history_count;
        }

        card->prev_rx_bytes = rx_bytes;
        card->prev_tx_bytes = tx_bytes;

        // Update rolling history
        if (card->history_count < HISTORY_LEN) {
            card->rx_history[card->history_count] = card->rx_kbs;
            card->history_count++;
        } else {
            memmove(&card->rx_history[0], &card->rx_history[1], sizeof(float) * (HISTORY_LEN - 1));
            card->rx_history[HISTORY_LEN - 1] = card->rx_kbs;
        }

        t->net_card_count++;
    }
    freeifaddrs(ifaddr);

    // Save state for next iteration
    s_saved_card_count = t->net_card_count;
    for (int i = 0; i < t->net_card_count; i++) {
        s_saved_cards[i] = t->net_cards[i];
    }
}

static void read_time_and_uptime(SystemTelemetry *t) {
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    strftime(t->time_str, sizeof(t->time_str), "%H:%M:%S", &tm_info);

    static const char *weekdays[] = { "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag" };
    static const char *months[] = { "Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez" };
    snprintf(t->date_str, sizeof(t->date_str), "%s, %02d. %s",
             weekdays[tm_info.tm_wday], tm_info.tm_mday, months[tm_info.tm_mon]);

    FILE *f = fopen("/proc/uptime", "r");
    if (f) {
        double up_secs = 0.0;
        if (fscanf(f, "%lf", &up_secs) == 1) {
            long s = (long)up_secs;
            int days = s / 86400;
            int hours = (s % 86400) / 3600;
            int mins = (s % 3600) / 60;
            if (days > 0) {
                snprintf(t->uptime_str, sizeof(t->uptime_str), "Uptime: %dd %dh %dm", days, hours, mins);
            } else {
                snprintf(t->uptime_str, sizeof(t->uptime_str), "Uptime: %02dh %02dm", hours, mins);
            }
        }
        fclose(f);
    }
}

void telemetry_update(SystemTelemetry *t) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double dt = (now.tv_sec - prev_time.tv_sec) + (now.tv_nsec - prev_time.tv_nsec) / 1e9;
    prev_time = now;

    read_cpu_temp_and_freq(t);
    read_fan_rpm(t);
    read_cpu_usage(t);
    read_gpu_telemetry(t);
    read_advanced_sensors(t);
    read_raid_and_sd(t);
    read_mem_usage(t);
    read_disk_usage_and_io(t, dt);
    read_network(t, dt);
    read_time_and_uptime(t);

    // Update rolling histories
    if (t->history_count < HISTORY_LEN) {
        t->temp_history[t->history_count] = t->cpu_temp;
        t->cpu_history[t->history_count] = t->cpu_usage;
        t->history_count++;
    } else {
        memmove(&t->temp_history[0], &t->temp_history[1], sizeof(float) * (HISTORY_LEN - 1));
        memmove(&t->cpu_history[0], &t->cpu_history[1], sizeof(float) * (HISTORY_LEN - 1));
        t->temp_history[HISTORY_LEN - 1] = t->cpu_temp;
        t->cpu_history[HISTORY_LEN - 1] = t->cpu_usage;
    }
}
