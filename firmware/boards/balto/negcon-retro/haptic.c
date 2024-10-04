#define DT_DRV_COMPAT ti_drv8212

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "event.h"

LOG_MODULE_REGISTER(haptic, LOG_LEVEL_INF);

static const struct gpio_dt_spec mode_gpio = GPIO_DT_SPEC_INST_GET(0, mode_gpios);
static const struct pwm_dt_spec pwm_phase = PWM_DT_SPEC_INST_GET_BY_IDX(0, 0);
static const struct pwm_dt_spec pwm_en = PWM_DT_SPEC_INST_GET_BY_IDX(0, 1);

static void haptic_cb(enum event_code code)
{
	switch (code) {
	case EVENT_BOOT:
	case EVENT_CHARGER_CHARGING:
	case EVENT_BT_DISCONNECTED:
		pwm_set_pulse_dt(&pwm_en, pwm_en.period);
		pwm_set_pulse_dt(&pwm_phase, pwm_phase.period / 2);
		k_sleep(K_MSEC(10));
		pwm_set_pulse_dt(&pwm_en, 0);
		pwm_set_pulse_dt(&pwm_phase, 0);
		break;
	case EVENT_BT_CONNECTED:
		pwm_set_pulse_dt(&pwm_en, pwm_en.period);
		pwm_set_pulse_dt(&pwm_phase, pwm_phase.period / 2);
		k_sleep(K_MSEC(10));
		pwm_set_pulse_dt(&pwm_en, 0);
		pwm_set_pulse_dt(&pwm_phase, 0);

		k_sleep(K_MSEC(100));

		pwm_set_pulse_dt(&pwm_en, pwm_en.period);
		pwm_set_pulse_dt(&pwm_phase, pwm_phase.period / 2);
		k_sleep(K_MSEC(10));
		pwm_set_pulse_dt(&pwm_en, 0);
		pwm_set_pulse_dt(&pwm_phase, 0);
		break;
	default:
	}
}
EVENT_CALLBACK_DEFINE(haptic_cb);

static int haptic_init(void)
{
	gpio_pin_configure_dt(&mode_gpio, GPIO_OUTPUT_HIGH);

	return 0;
}
SYS_INIT(haptic_init, APPLICATION, 99);
