#define DT_DRV_COMPAT charger_gpio

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "event.h"

LOG_MODULE_REGISTER(charger_gpio, LOG_LEVEL_INF);

static const struct gpio_dt_spec chg_gpio = GPIO_DT_SPEC_INST_GET(0, gpios);

static void charging_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(charging_dwork, charging_handler);

static void charging_handler(struct k_work *work)
{
	static enum event_code last_code = EVENT_CHARGER_DISCONNECTED;
	static enum event_code code;
	int val;

	val = gpio_pin_get_dt(&chg_gpio);

	if (val) {
		code = EVENT_CHARGER_CHARGING;
	} else {
		code = EVENT_CHARGER_DISCONNECTED;
	}

	if (code != last_code) {
		event(code);
		last_code = code;
	}

	k_work_schedule(&charging_dwork, K_MSEC(1000));
}

static int charging_start(void)
{
	int ret;

	ret = gpio_pin_configure_dt(&chg_gpio, GPIO_INPUT);
	if (ret < 0) {
		return 0;
	}

	charging_handler(&charging_dwork.work);

	return 0;
}
SYS_INIT(charging_start, APPLICATION, 99);
