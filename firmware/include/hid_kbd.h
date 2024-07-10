#include <zephyr/device.h>
#include <stdint.h>

typedef void (*hid_kbd_led_cb)(const struct device *dev, uint8_t state);

void hid_kbd_register_led_cb(const struct device *dev, hid_kbd_led_cb cb);
