#include <zephyr/drivers/led.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <hid_kbd.h>

static const struct led_dt_spec led_caps = LED_DT_SPEC_GET(DT_NODELABEL(led_capslock));
static const struct device *hid_kbd = DEVICE_DT_GET_ONE(hid_kbd);

static void caps_led_cb(const struct device *dev, uint8_t state)
{
	if (state & BIT(1)) {
		led_on_dt(&led_caps);
	} else {
		led_off_dt(&led_caps);
	}
}

static int caps_led_setup(void)
{
	hid_kbd_register_led_cb(hid_kbd, caps_led_cb);
	return 0;
}
SYS_INIT(caps_led_setup, APPLICATION, 99);
