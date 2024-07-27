#include <zephyr/input/input.h>
#include <zephyr/input/input_pmw3610.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pmw_power, LOG_LEVEL_INF);

#define PMW_NODE DT_NODELABEL(pmw)

#define FORCE_AWAKE_TIMEOUT_S 30

static const struct device *const pmw = DEVICE_DT_GET(PMW_NODE);

static bool force_awake;

static void sleep_handler(struct k_work *work)
{
	pmw3610_force_awake(pmw, false);
	force_awake = false;

	LOG_DBG("sleep");
}

K_WORK_DELAYABLE_DEFINE(sleep_dwork, sleep_handler);

static void pmw_cb(struct input_event *evt, void *user_data)
{
	if (!force_awake) {
		pmw3610_force_awake(pmw, true);
		force_awake = true;
		LOG_DBG("awake");
	}

	k_work_reschedule(&sleep_dwork, K_SECONDS(FORCE_AWAKE_TIMEOUT_S));
}
INPUT_CALLBACK_DEFINE(pmw, pmw_cb, NULL);
