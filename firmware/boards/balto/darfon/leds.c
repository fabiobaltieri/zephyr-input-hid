#include <zephyr/drivers/led.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <hid_kbd.h>

#define LED_CAPS_NODE DT_NODELABEL(led_capslock)
#define LED_CAPS DT_NODE_CHILD_IDX(LED_CAPS_NODE)
static const struct device *leds = DEVICE_DT_GET(DT_PARENT(LED_CAPS_NODE));
static const struct device *hid_kbd = DEVICE_DT_GET_ONE(hid_kbd);

static void caps_led_cb(const struct device *dev, uint8_t state)
{
	if (state & BIT(1)) {
		led_on(leds, LED_CAPS);
	} else {
		led_off(leds, LED_CAPS);
	}
}

static int caps_led_setup(void)
{
	hid_kbd_register_led_cb(hid_kbd, caps_led_cb);
	return 0;
}
SYS_INIT(caps_led_setup, APPLICATION, 99);
