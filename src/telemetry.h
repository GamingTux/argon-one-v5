#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>

#define HISTORY_LEN 60
#define MAX_NET_CARDS 4

typedef struct {
    char name[16];
    char ip[32];
    char speed_desc[24];
    char ssid[34];
    bool is_wifi;
    bool is_usb;
    float rx_kbs;
    float tx_kbs;
    float rx_history[HISTORY_LEN];
    int history_count;
    unsigned long long prev_rx_bytes;
    unsigned long long prev_tx_bytes;
} NetCardTelemetry;

typedef struct {
    float cpu_temp;
    float cpu_freq_ghz;
    float cpu_usage;
    float core_usage[4];
    int   fan_rpm;

    // GPU Telemetry (VideoCore VII)
    bool  has_gpu;
    float gpu_usage;
    int   gpu_freq_mhz;

    // Advanced Hardware Sensoren
    float nvme_temp;
    float rp1_temp;
    bool  undervoltage_alarm;
    float in_5v_volt;

    int   ram_used_mb;
    int   ram_total_mb;
    float ram_percent;

    // Storage & Multi-NVMe / RAID
    float disk_used_gb;
    float disk_total_gb;
    float disk_percent;
    float disk_read_mbs;
    float disk_write_mbs;
    char  disk_dev_name[32];
    char  disk_label[32];

    bool  raid_active;
    bool  raid_degraded;
    char  raid_level[32];
    char  raid_health[32];

    // MicroSD slot
    bool  sd_present;
    float sd_size_gb;

    // USB storage detection
    bool  usb_mounted;
    float usb_used_gb;
    float usb_total_gb;
    float usb_percent;
    char  usb_name[64];
    float usb_read_mbs;
    float usb_write_mbs;

    // Equalizer levels & peak-hold dots for Storage & Memory (7 columns)
    int   nvme_eq_heights[7];
    int   nvme_eq_peaks[7];
    int   ram_eq_heights[7];
    int   ram_eq_peaks[7];
    int   usb_eq_heights[7];
    int   usb_eq_peaks[7];

    // Dynamic active network cards
    NetCardTelemetry net_cards[MAX_NET_CARDS];
    int net_card_count;

    char  uptime_str[32];
    char  time_str[16];
    char  date_str[32];

    float temp_history[HISTORY_LEN];
    float cpu_history[HISTORY_LEN];
    int   history_count;
} SystemTelemetry;

void telemetry_init(void);
void telemetry_update(SystemTelemetry *t);

#endif // TELEMETRY_H
