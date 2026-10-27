#include "button.h"

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <poll.h>

static pthread_t thread_id;
static volatile int running = 0;
static ButtonCallback callback = NULL;

static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *request = NULL;
static struct gpiod_edge_event_buffer *event_buf = NULL;

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}

static void *button_thread(void *arg) {
    (void)arg;
    int fd = gpiod_line_request_get_fd(request);
    struct pollfd pfd = { .fd = fd, .events = POLLIN };

    uint64_t press_start_ms = 0;
    uint64_t last_click_ms = 0;
    int pending_click = 0;

    while (running) {
        int timeout_ms = pending_click ? 300 : 1000;
        int ret = poll(&pfd, 1, timeout_ms);

        uint64_t now = get_time_ms();

        // Check if a single click timed out without a double click
        if (pending_click && (now - last_click_ms > 350)) {
            if (callback) callback(BTN_EVENT_SHORT_CLICK);
            pending_click = 0;
        }

        if (ret < 0) {
            if (!running) break;
            continue;
        }

        if (ret > 0 && (pfd.revents & POLLIN)) {
            int events_read = gpiod_line_request_read_edge_events(request, event_buf, 16);
            for (int i = 0; i < events_read; i++) {
                struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(event_buf, i);
                int type = gpiod_edge_event_get_event_type(ev);

                if (type == GPIOD_EDGE_EVENT_FALLING_EDGE) {
                    // Button pressed down (active-low)
                    press_start_ms = get_time_ms();
                } else if (type == GPIOD_EDGE_EVENT_RISING_EDGE) {
                    // Button released
                    if (press_start_ms > 0) {
                        uint64_t duration = get_time_ms() - press_start_ms;
                        press_start_ms = 0;

                        if (duration >= 5000) {
                            if (callback) callback(BTN_EVENT_LONG_HOLD);
                            pending_click = 0;
                        } else if (duration >= 1200) {
                            if (callback) callback(BTN_EVENT_MEDIUM_HOLD);
                            pending_click = 0;
                        } else if (duration >= 20) {
                            // Short press
                            if (pending_click && (now - last_click_ms <= 350)) {
                                if (callback) callback(BTN_EVENT_DOUBLE_CLICK);
                                pending_click = 0;
                            } else {
                                pending_click = 1;
                                last_click_ms = now;
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

int button_init(const char *gpio_chip, unsigned int offset, ButtonCallback cb) {
    callback = cb;

    chip = gpiod_chip_open(gpio_chip);
    if (!chip) {
        perror("button: gpiod_chip_open failed");
        return -1;
    }

    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_debounce_period_us(settings, 15000); // 15ms debounce

    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    unsigned int offsets[1] = { offset };
    gpiod_line_config_add_line_settings(line_cfg, offsets, 1, settings);

    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "argon-v5-oled");

    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if (!request) {
        perror("button: gpiod_chip_request_lines failed");
        gpiod_chip_close(chip);
        chip = NULL;
        return -1;
    }

    event_buf = gpiod_edge_event_buffer_new(16);

    running = 1;
    if (pthread_create(&thread_id, NULL, button_thread, NULL) != 0) {
        perror("button: pthread_create failed");
        button_stop();
        return -1;
    }

    return 0;
}

void button_stop(void) {
    if (running) {
        running = 0;
        pthread_join(thread_id, NULL);
    }
    if (event_buf) {
        gpiod_edge_event_buffer_free(event_buf);
        event_buf = NULL;
    }
    if (request) {
        gpiod_line_request_release(request);
        request = NULL;
    }
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}
