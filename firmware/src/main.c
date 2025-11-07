#include <zephyr/drivers/led.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "event.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static struct k_sem blink_sem = Z_SEM_INITIALIZER(blink_sem, 0, 4);

#define BLINKER_NODE DT_NODELABEL(led_input_activity)
#define STATUS_NODE DT_NODELABEL(led_status)

static const struct led_dt_spec activity_led = LED_DT_SPEC_GET_OR(BLINKER_NODE, {0});
static const struct led_dt_spec status_led = LED_DT_SPEC_GET_OR(STATUS_NODE, {0});

#define BLINKER_QUEUE_SZ 10

K_MSGQ_DEFINE(blinker_msgq, sizeof(enum event_code), BLINKER_QUEUE_SZ, 4);

static bool led_invert;

static void led_status(bool on)
{
	if (!led_is_ready_dt(&status_led)) {
		return;
	}

	if (on) {
		led_set_brightness_dt(&status_led,
				      (activity_led.dev == NULL && led_invert) ? 0 : 100);
	} else {
		led_set_brightness_dt(&status_led,
				      (activity_led.dev == NULL && led_invert) ? 100 : 0);
	}
}

static void led_activity(bool on)
{
	if (!led_is_ready_dt(&activity_led)) {
		led_status(on);
		return;
	}

	if (on) {
		led_set_brightness_dt(&activity_led, led_invert ? 0 : 100);
	} else {
		led_set_brightness_dt(&activity_led, led_invert ? 100 : 0);
	}
}

int main(void)
{
	enum event_code code;
	int ret;

	event(EVENT_BOOT);

	while (true) {
		ret = k_msgq_get(&blinker_msgq, &code, K_FOREVER);
		if (ret) {
			LOG_ERR("k_msgq_get error: %d", ret);
			continue;
		}

		if (!led_is_ready_dt(&status_led) && !led_is_ready_dt(&activity_led)) {
			continue;
		}

		switch (code) {
		case EVENT_BOOT:
			led_status(true);
			k_sleep(K_MSEC(100));
			led_status(false);
			break;
		case EVENT_BT_CONNECTED:
			led_status(true);
			k_sleep(K_MSEC(100));
			led_status(false);
			k_sleep(K_MSEC(150));
			led_status(true);
			k_sleep(K_MSEC(100));
			led_status(false);
			k_sleep(K_MSEC(150));
			led_status(true);
			k_sleep(K_MSEC(100));
			led_status(false);
			break;
		case EVENT_BT_DISCONNECTED:
			led_status(true);
			k_sleep(K_MSEC(300));
			led_status(false);
			break;
		case EVENT_BT_UNPAIRED:
			led_status(true);
			k_sleep(K_SECONDS(3));
			led_status(false);
			break;
		case EVENT_INPUT:
			led_activity(true);
			k_sleep(K_MSEC(30));
			led_activity(false);
			k_sleep(K_MSEC(30));
			break;
		case EVENT_POWEROFF:
			led_status(true);
			k_sleep(K_SECONDS(1));
			led_status(false);
			k_sleep(K_MSEC(200));
			led_status(true);
			k_sleep(K_MSEC(50));
			led_status(false);
			k_sleep(K_MSEC(200));
			led_status(true);
			k_sleep(K_MSEC(50));
			led_status(false);
			break;
		case EVENT_CHARGER_CHARGING:
		case EVENT_CHARGER_FULL:
		case EVENT_CHARGER_DISCONNECTED:
			led_activity(false);
			break;
		default:
			break;
		}
	}

	return 0;
}

static void charger_cb(enum event_code code)
{
	switch (code) {
		case EVENT_CHARGER_CHARGING:
			led_invert = true;
			break;
		case EVENT_CHARGER_FULL:
		case EVENT_CHARGER_DISCONNECTED:
			led_invert = false;
			break;
		default:
			break;
	}
}
EVENT_CALLBACK_DEFINE(charger_cb);

static void blinker_cb(enum event_code code)
{
	int ret;

	ret = k_msgq_put(&blinker_msgq, &code, K_NO_WAIT);
	if (ret != 0) {
		LOG_DBG("blinker queue full");
	}

	LOG_DBG("event %d", code);
}
EVENT_CALLBACK_DEFINE(blinker_cb);

static void blink_input_cb(struct input_event *evt, void *user_data)
{
#if CONFIG_APP_EVT_BLINK_TIMEOUT
	if (k_uptime_get() > 60 * 1000) {
		return;
	}
#endif

	if (!evt->sync) {
		return;
	}

	event(EVENT_INPUT);
}
INPUT_CALLBACK_DEFINE(NULL, blink_input_cb, NULL);
