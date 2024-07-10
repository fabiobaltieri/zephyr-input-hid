#include <zephyr/drivers/led.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#define LED_CLICKER_NODE DT_NODELABEL(led_clicker)
#define LED_CLICKER DT_NODE_CHILD_IDX(LED_CLICKER_NODE)
static const struct device *leds = DEVICE_DT_GET(DT_PARENT(LED_CLICKER_NODE));

static bool clicker_enabled;

static const struct pwm_dt_spec clicker = PWM_DT_SPEC_GET(DT_NODELABEL(clicker));

static void click_handler(struct k_work *work)
{
        pwm_set_pulse_dt(&clicker, 0);
}
static K_WORK_DELAYABLE_DEFINE(click_dwork, click_handler);

static void clicker_cb(struct input_event *evt)
{
	if (!evt->sync) {
		return;
	}

	if (!clicker_enabled) {
		return;
	}

	if (evt->value != 1) {
		return;
	}

	pwm_set_pulse_dt(&clicker, clicker.period);
	k_work_reschedule(&click_dwork, K_MSEC(30));
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(keymap)), clicker_cb);

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

	if (row == 5 && col == 0 && val == 1) {
		sys_reboot(0);
	} else if (row == 4 && col == 16 && val == 1) {
		clicker_enabled = !clicker_enabled;
		led_set_brightness(leds, LED_CLICKER, clicker_enabled ? 100: 0);
	}
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(kbd)), input_cb);
