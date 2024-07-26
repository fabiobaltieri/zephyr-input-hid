#include <zephyr/drivers/led.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "event.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static struct k_sem blink_sem = Z_SEM_INITIALIZER(blink_sem, 0, 4);

#define BLINKER_NODE DT_NODELABEL(led_input_activity)
#define STATUS_NODE DT_NODELABEL(led_status)

static const struct {
	const struct device *activity_leds;
	const uint32_t activity_idx;
	const struct device *status_leds;
	const uint32_t status_idx;
} cfg = {
#if DT_NODE_EXISTS(BLINKER_NODE)
	.activity_leds = DEVICE_DT_GET_OR_NULL(DT_PARENT(BLINKER_NODE)),
	.activity_idx = DT_NODE_CHILD_IDX(BLINKER_NODE),
#endif
#if DT_NODE_EXISTS(STATUS_NODE)
	.status_leds = DEVICE_DT_GET(DT_PARENT(STATUS_NODE)),
	.status_idx = DT_NODE_CHILD_IDX(STATUS_NODE),
#endif
};

#define BLINKER_QUEUE_SZ 10

K_MSGQ_DEFINE(blinker_msgq, sizeof(enum event_code), BLINKER_QUEUE_SZ, 4);

static void led_status(bool on)
{
	if (cfg.status_leds == NULL) {
		return;
	}

	if (on) {
		led_on(cfg.status_leds, cfg.status_idx);
	} else {
		led_off(cfg.status_leds, cfg.status_idx);
	}
}

static void led_activity(bool on)
{
	if (cfg.activity_leds == NULL) {
		led_status(on);
		return;
	}

	if (on) {
		led_on(cfg.activity_leds, cfg.activity_idx);
	} else {
		led_off(cfg.activity_leds, cfg.activity_idx);
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

		if (cfg.status_leds == NULL && cfg.activity_leds == NULL) {
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
		default:
			break;
		}
	}

	return 0;
}

void blinker_cb(enum event_code code)
{
	int ret;

	ret = k_msgq_put(&blinker_msgq, &code, K_NO_WAIT);
	if (ret != 0) {
		LOG_WRN("blinker queue full");
	}

	LOG_DBG("event %d", code);
}
EVENT_CALLBACK_DEFINE(blinker_cb);

static void blink_input_cb(struct input_event *evt)
{
	if (!evt->sync) {
		return;
	}

	event(EVENT_INPUT);
}
INPUT_CALLBACK_DEFINE(NULL, blink_input_cb);
