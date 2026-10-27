#ifndef BUTTON_H
#define BUTTON_H

typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_SHORT_CLICK,
    BTN_EVENT_DOUBLE_CLICK,
    BTN_EVENT_MEDIUM_HOLD,
    BTN_EVENT_LONG_HOLD
} ButtonEvent;

typedef void (*ButtonCallback)(ButtonEvent event);

int  button_init(const char *gpio_chip, unsigned int offset, ButtonCallback cb);
void button_stop(void);

#endif // BUTTON_H
