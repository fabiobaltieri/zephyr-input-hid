#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>

#include "ble.h"
#include "blinker.h"

LOG_MODULE_REGISTER(auto_poweroff, LOG_LEVEL_INF);

#define AUTO_POWEROFF_TIME K_SECONDS(60 * 15)

static const struct gpio_dt_spec wkup = GPIO_DT_SPEC_GET(DT_NODELABEL(wkup), gpios);

static void auto_poweroff_handler(struct k_work *work)
{
	LOG_INF("auto poweroff");

	ble_stop();

	k_sleep(K_SECONDS(1));

	blink(BLINK_POWEROFF);

	k_sleep(K_SECONDS(2));

	gpio_pin_configure_dt(&wkup, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&wkup, GPIO_INT_LEVEL_ACTIVE);

	sys_poweroff();
}

static K_WORK_DELAYABLE_DEFINE(auto_poweroff_dwork, auto_poweroff_handler);

static void auto_poweroff_cb(struct input_event *evt)
{
	if (evt->type == INPUT_EV_ABS) {
		return;
	}

	k_work_reschedule(&auto_poweroff_dwork, AUTO_POWEROFF_TIME);
}
INPUT_CALLBACK_DEFINE(NULL, auto_poweroff_cb);

static int auto_poweroff_init(void)
{
	k_work_reschedule(&auto_poweroff_dwork, AUTO_POWEROFF_TIME);

	return 0;
}
SYS_INIT(auto_poweroff_init, APPLICATION, 99);
