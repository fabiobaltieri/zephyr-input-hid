#include <zephyr/drivers/led.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

static void input_cb(struct input_event *evt, void *user_data)
{
	static const struct led_dt_spec touch_led = LED_DT_SPEC_GET(DT_NODELABEL(led_touch));

	if (evt->code == INPUT_BTN_TOUCH) {
		if (evt->value) {
			led_on_dt(&touch_led);
		} else {
			led_off_dt(&touch_led);
		}
	}
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(touch_sense)), input_cb, NULL);
