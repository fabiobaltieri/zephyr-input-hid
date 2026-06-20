#include <zephyr/input/input.h>
#include <zephyr/input/input_paw32xx.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(paw_power, LOG_LEVEL_INF);

#define PAW_NODE DT_NODELABEL(paw)

#define FORCE_AWAKE_TIMEOUT_S (5 * 60)

static const struct device *const paw = DEVICE_DT_GET(DT_NODELABEL(paw));
static const struct device *const buttons = DEVICE_DT_GET(DT_NODELABEL(buttons));

static bool force_awake;

static void sleep_handler(struct k_work *work)
{
	paw32xx_force_awake(paw, false);
	force_awake = false;

	LOG_INF("sleep");
}

K_WORK_DELAYABLE_DEFINE(sleep_dwork, sleep_handler);

static void power_btn_cb(struct input_event *evt, void *user_data)
{
	if (evt->code != INPUT_BTN_TASK) {
		return;
	}

	if (evt->value == 0) {
		return;
	}

	if (force_awake) {
		return;
	}

	paw32xx_force_awake(paw, true);
	force_awake = true;
	LOG_INF("awake");

	k_work_reschedule(&sleep_dwork, K_SECONDS(FORCE_AWAKE_TIMEOUT_S));
}
INPUT_CALLBACK_DEFINE(buttons, power_btn_cb, NULL);

static void power_paw_cb(struct input_event *evt, void *user_data)
{
	if (!evt->sync) {
		return;
	}

	if (!force_awake) {
		return;
	}

	k_work_reschedule(&sleep_dwork, K_SECONDS(FORCE_AWAKE_TIMEOUT_S));
}
INPUT_CALLBACK_DEFINE(paw, power_paw_cb, NULL);
