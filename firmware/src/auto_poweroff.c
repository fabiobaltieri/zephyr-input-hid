#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>

#include "ble.h"
#include "event.h"

LOG_MODULE_REGISTER(auto_poweroff, LOG_LEVEL_INF);

#define AUTO_POWEROFF_TIME K_SECONDS(60 * 15)

static const struct gpio_dt_spec wkup = GPIO_DT_SPEC_GET(DT_NODELABEL(wkup), gpios);

static const struct device *analog_pwr = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(analog_pwr));

#if CONFIG_DT_HAS_SUSPEND_GPIOS_ENABLED
#define SUSPEND_GPIOS_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(suspend_gpios)
static const struct gpio_dt_spec suspend_gpio_specs[] = {
	DT_FOREACH_PROP_ELEM_SEP(SUSPEND_GPIOS_NODE, gpios,
				 GPIO_DT_SPEC_GET_BY_IDX, (,))
};

static void suspend_gpios(void)
{
	int ret;

	for (int i = 0; i < ARRAY_SIZE(suspend_gpio_specs); i++) {
		ret = gpio_pin_configure_dt(&suspend_gpio_specs[i], GPIO_INPUT);
		if (ret != 0) {
			LOG_ERR("Pin %d configuration failed: %d", i, ret);
			return;
		}
	}
}
#else
static void suspend_gpios(void)
{
}
#endif

static void auto_poweroff_handler(struct k_work *work)
{
	LOG_INF("auto poweroff");

	ble_stop();

	k_sleep(K_SECONDS(1));

	event(EVENT_POWEROFF);

	k_sleep(K_SECONDS(2));

	suspend_gpios();

	if (analog_pwr != NULL) {
		regulator_disable(analog_pwr);
	}

	gpio_pin_configure_dt(&wkup, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&wkup, GPIO_INT_LEVEL_ACTIVE);

	sys_poweroff();
}

static K_WORK_DELAYABLE_DEFINE(auto_poweroff_dwork, auto_poweroff_handler);

static void auto_poweroff_cb(struct input_event *evt, void *user_data)
{
	if (evt->type == INPUT_EV_ABS) {
		return;
	}

	k_work_reschedule(&auto_poweroff_dwork, AUTO_POWEROFF_TIME);
}
INPUT_CALLBACK_DEFINE(NULL, auto_poweroff_cb, NULL);

static int auto_poweroff_init(void)
{
	k_work_reschedule(&auto_poweroff_dwork, AUTO_POWEROFF_TIME);

	return 0;
}
SYS_INIT(auto_poweroff_init, APPLICATION, 99);
