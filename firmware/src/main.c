#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/drivers/led.h>

static struct k_sem blink_sem = Z_SEM_INITIALIZER(blink_sem, 0, 10);

static void input_cb(struct input_event *evt)
{
	static int col, row, val;

	switch (evt->code) {
	case INPUT_ABS_X:
		col = evt->value;
		break;
	case INPUT_ABS_Y:
		row = evt->value;
		break;
	case INPUT_BTN_TOUCH:
		val = evt->value;
		break;
	}

	if (!evt->sync) {
		return;
	}

	k_sem_give(&blink_sem);
}
INPUT_CALLBACK_DEFINE(NULL, input_cb);

int main(void)
{
	static const struct device *leds = DEVICE_DT_GET(
			DT_COMPAT_GET_ANY_STATUS_OKAY(gpio_leds));

	if (!device_is_ready(leds)) {
		return -ENODEV;
	}

	while (true) {
		k_sem_take(&blink_sem, K_FOREVER);

		led_on(leds, 0);
		k_sleep(K_MSEC(30));
		led_off(leds, 0);
		k_sleep(K_MSEC(30));
	}

	return 0;
}
