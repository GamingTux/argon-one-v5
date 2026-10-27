#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BTN_ACT_NEXT = 0,
    BTN_ACT_PREV,
    BTN_ACT_TOGGLE_SLEEP,
    BTN_ACT_TOGGLE_DIM,
    BTN_ACT_SHUTDOWN,
    BTN_ACT_REBOOT,
    BTN_ACT_NONE
} ButtonAction;

typedef struct {
    char     i2c_bus[64];
    uint8_t  i2c_address;
    char     gpio_chip[64];
    uint32_t gpio_pin;
    int      screen_duration_secs;
    int      screensaver_timeout_secs;
    uint8_t  brightness;
    uint8_t  dim_brightness;
    float    temp_alert_threshold;
    bool     enable_pixel_shift;
    ButtonAction btn_action_short;
    ButtonAction btn_action_long;
    int      variant_cpu;   // 0=A, 1=B, 2=C
    int      variant_mem;   // 0=A, 1=B, 2=C
    int      variant_net;   // 0=A, 1=B, 2=C
    int      variant_clock; // 0=A, 1=B, 2=C
} DaemonConfig;

void config_load_defaults(DaemonConfig *cfg);
int  config_load_file(const char *path, DaemonConfig *cfg);
int  config_save_file(const char *path, const DaemonConfig *cfg);
const char *button_action_to_str(ButtonAction act);
ButtonAction parse_button_action(const char *val);

#endif // CONFIG_H
