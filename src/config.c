#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void config_load_defaults(DaemonConfig *cfg) {
    strncpy(cfg->i2c_bus, "/dev/i2c-1", sizeof(cfg->i2c_bus) - 1);
    cfg->i2c_address = 0x3c;
    strncpy(cfg->gpio_chip, "/dev/gpiochip0", sizeof(cfg->gpio_chip) - 1);
    cfg->gpio_pin = 4;
    cfg->screen_duration_secs = 10;
    cfg->screensaver_timeout_secs = 120;
    cfg->brightness = 200;
    cfg->dim_brightness = 25;
    cfg->temp_alert_threshold = 80.0f;
    cfg->enable_pixel_shift = false;
    cfg->btn_action_short = BTN_ACT_NEXT;
    cfg->btn_action_long = BTN_ACT_TOGGLE_SLEEP;
    cfg->variant_cpu = 0;   // A
    cfg->variant_mem = 2;   // C (Storage & Memory Matrix)
    cfg->variant_net = 0;   // A (Inverted Solid IP & Wave)
    cfg->variant_clock = 0; // A (Centered Hero Clock)
}

static int parse_variant(const char *val) {
    if (strcasecmp(val, "b") == 0 || strcmp(val, "1") == 0) return 1;
    if (strcasecmp(val, "c") == 0 || strcmp(val, "2") == 0) return 2;
    return 0; // default 'A'
}

ButtonAction parse_button_action(const char *val) {
    if (!val) return BTN_ACT_NEXT;
    if (strcasecmp(val, "next") == 0) return BTN_ACT_NEXT;
    if (strcasecmp(val, "prev") == 0) return BTN_ACT_PREV;
    if (strcasecmp(val, "toggle_sleep") == 0 || strcasecmp(val, "sleep") == 0 || strcasecmp(val, "power") == 0) return BTN_ACT_TOGGLE_SLEEP;
    if (strcasecmp(val, "toggle_dim") == 0 || strcasecmp(val, "dim") == 0) return BTN_ACT_TOGGLE_DIM;
    if (strcasecmp(val, "shutdown") == 0 || strcasecmp(val, "poweroff") == 0) return BTN_ACT_SHUTDOWN;
    if (strcasecmp(val, "reboot") == 0 || strcasecmp(val, "restart") == 0) return BTN_ACT_REBOOT;
    if (strcasecmp(val, "none") == 0 || strcasecmp(val, "off") == 0 || strcasecmp(val, "disable") == 0) return BTN_ACT_NONE;
    return BTN_ACT_NEXT;
}

const char *button_action_to_str(ButtonAction act) {
    switch (act) {
        case BTN_ACT_NEXT: return "next";
        case BTN_ACT_PREV: return "prev";
        case BTN_ACT_TOGGLE_SLEEP: return "toggle_sleep";
        case BTN_ACT_TOGGLE_DIM: return "toggle_dim";
        case BTN_ACT_SHUTDOWN: return "shutdown";
        case BTN_ACT_REBOOT: return "reboot";
        case BTN_ACT_NONE: return "none";
        default: return "none";
    }
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

int config_load_file(const char *path, DaemonConfig *cfg) {
    config_load_defaults(cfg);

    FILE *f = fopen(path, "r");
    if (!f) return -1; // File optional, defaults used

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '#' || *p == ';' || *p == '[' || *p == '\0') continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        if (strcasecmp(key, "i2c_bus") == 0) {
            strncpy(cfg->i2c_bus, val, sizeof(cfg->i2c_bus) - 1);
        } else if (strcasecmp(key, "i2c_address") == 0) {
            cfg->i2c_address = (uint8_t)strtoul(val, NULL, 0);
        } else if (strcasecmp(key, "gpio_chip") == 0) {
            strncpy(cfg->gpio_chip, val, sizeof(cfg->gpio_chip) - 1);
        } else if (strcasecmp(key, "gpio_pin") == 0) {
            cfg->gpio_pin = (uint32_t)strtoul(val, NULL, 0);
        } else if (strcasecmp(key, "screen_duration") == 0) {
            cfg->screen_duration_secs = atoi(val);
        } else if (strcasecmp(key, "screensaver_timeout") == 0) {
            cfg->screensaver_timeout_secs = atoi(val);
        } else if (strcasecmp(key, "brightness") == 0) {
            cfg->brightness = (uint8_t)atoi(val);
        } else if (strcasecmp(key, "dim_brightness") == 0) {
            cfg->dim_brightness = (uint8_t)atoi(val);
        } else if (strcasecmp(key, "temp_alert_threshold") == 0) {
            cfg->temp_alert_threshold = (float)atof(val);
        } else if (strcasecmp(key, "enable_pixel_shift") == 0) {
            cfg->enable_pixel_shift = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcasecmp(val, "yes") == 0);
        } else if (strcasecmp(key, "btn_action_short") == 0) {
            cfg->btn_action_short = parse_button_action(val);
        } else if (strcasecmp(key, "btn_action_long") == 0) {
            cfg->btn_action_long = parse_button_action(val);
        } else if (strcasecmp(key, "variant_cpu") == 0) {
            cfg->variant_cpu = parse_variant(val);
        } else if (strcasecmp(key, "variant_mem") == 0) {
            cfg->variant_mem = parse_variant(val);
        } else if (strcasecmp(key, "variant_net") == 0) {
            cfg->variant_net = parse_variant(val);
        } else if (strcasecmp(key, "variant_clock") == 0) {
            cfg->variant_clock = parse_variant(val);
        }
    }

    fclose(f);
    return 0;
}

int config_save_file(const char *path, const DaemonConfig *cfg) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "#\n# Argon ONE V5 OLED & Button Daemon Configuration\n#\n\n");
    fprintf(f, "# I2C bus and address for SSD1306 OLED display\n");
    fprintf(f, "i2c_bus = %s\n", cfg->i2c_bus);
    fprintf(f, "i2c_address = 0x%02x\n\n", cfg->i2c_address);

    fprintf(f, "# GPIO chip and pin for physical button on OLED assembly\n");
    fprintf(f, "gpio_chip = %s\n", cfg->gpio_chip);
    fprintf(f, "gpio_pin = %u\n\n", cfg->gpio_pin);

    fprintf(f, "# Seconds to display each screen before cycling (0 = manual only via button)\n");
    fprintf(f, "screen_duration = %d\n\n", cfg->screen_duration_secs);

    fprintf(f, "# Seconds of idle before turning off OLED (0 = always on)\n");
    fprintf(f, "screensaver_timeout = %d\n\n", cfg->screensaver_timeout_secs);

    fprintf(f, "# Display brightness (0 to 255)\n");
    fprintf(f, "brightness = %u\n\n", cfg->brightness);

    fprintf(f, "# Dimmed display brightness (0 to 255)\n");
    fprintf(f, "dim_brightness = %u\n\n", cfg->dim_brightness);

    fprintf(f, "# Temperature alert threshold in °C\n");
    fprintf(f, "temp_alert_threshold = %.1f\n\n", cfg->temp_alert_threshold);

    fprintf(f, "# Subtle pixel shift (anti-burn-in protection)\n");
    fprintf(f, "enable_pixel_shift = %s\n\n", cfg->enable_pixel_shift ? "true" : "false");

    fprintf(f, "# Hardware button action configuration\n");
    fprintf(f, "# Options: next, prev, toggle_sleep, toggle_dim, shutdown, reboot, none\n");
    fprintf(f, "btn_action_short = %s\n", button_action_to_str(cfg->btn_action_short));
    fprintf(f, "btn_action_long = %s\n\n", button_action_to_str(cfg->btn_action_long));

    fprintf(f, "# Selected card layout variants (a, b, c)\n");
    fprintf(f, "variant_cpu = %c\n", 'a' + cfg->variant_cpu);
    fprintf(f, "variant_mem = %c\n", 'a' + cfg->variant_mem);
    fprintf(f, "variant_net = %c\n", 'a' + cfg->variant_net);
    fprintf(f, "variant_clock = %c\n", 'a' + cfg->variant_clock);

    fclose(f);
    return 0;
}
