#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "blinker.h"

LOG_MODULE_REGISTER(blinker, LOG_LEVEL_INF);

#define LED_SYSTEM_NODE DT_NODELABEL(system)
#define LED_SYSTEM DT_NODE_CHILD_IDX(LED_SYSTEM_NODE)
static const struct device *leds = DEVICE_DT_GET(DT_PARENT(LED_SYSTEM_NODE));

#define BLINKER_QUEUE_SZ 10

K_MSGQ_DEFINE(blinker_msgq, sizeof(enum blinker_event), BLINKER_QUEUE_SZ, 4);

void blink(enum blinker_event event)
{
	int ret;

	ret = k_msgq_put(&blinker_msgq, &event, K_NO_WAIT);
	if (ret != 0) {
		LOG_WRN("blinker queue full");
	}
}

static void blinker_thread(void)
{
	enum blinker_event event;
	int ret;

	if (!device_is_ready(leds)) {
		LOG_ERR("leds device is not ready");
		return;
	}

	led_on(leds, LED_SYSTEM);
	k_sleep(K_MSEC(100));
	led_off(leds, LED_SYSTEM);

	while (true) {
		ret = k_msgq_get(&blinker_msgq, &event, K_FOREVER);
		if (ret) {
			LOG_ERR("k_msgq_get error: %d", ret);
			continue;
		}

		switch (event) {
		case BLINK_CONNECTED:
			led_on(leds, LED_SYSTEM);
			k_sleep(K_MSEC(100));
			led_off(leds, LED_SYSTEM);
			k_sleep(K_MSEC(150));
			led_on(leds, LED_SYSTEM);
			k_sleep(K_MSEC(100));
			led_off(leds, LED_SYSTEM);
			k_sleep(K_MSEC(150));
			led_on(leds, LED_SYSTEM);
			k_sleep(K_MSEC(100));
			led_off(leds, LED_SYSTEM);
			break;
		case BLINK_DISCONNECTED:
			led_on(leds, LED_SYSTEM);
			k_sleep(K_MSEC(300));
			led_off(leds, LED_SYSTEM);
			break;
		case BLINK_UNPAIRED:
			led_on(leds, LED_SYSTEM);
			k_sleep(K_SECONDS(3));
			led_off(leds, LED_SYSTEM);
			break;
		}

		k_sleep(K_MSEC(100));
	}
}

K_THREAD_DEFINE(blinker, 384, blinker_thread, NULL, NULL, NULL, 7, 0, 0);
