#include <zephyr/drivers/led.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "blinker.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static struct k_sem blink_sem = Z_SEM_INITIALIZER(blink_sem, 0, 10);

#if DT_NODE_EXISTS(DT_NODELABEL(led_input_blink))
#define BLINKER_NODE DT_NODELABEL(led_input_blink)
static const struct device *leds = DEVICE_DT_GET(DT_PARENT(BLINKER_NODE));
static const uint32_t blinker_led = DT_NODE_CHILD_IDX(BLINKER_NODE);
#else
static const struct device *leds;
static const uint32_t blinker_led;
#endif

static void blink_input_cb(struct input_event *evt)
{
	if (!evt->sync) {
		return;
	}

	k_sem_give(&blink_sem);
}
INPUT_CALLBACK_DEFINE(NULL, blink_input_cb);

int main(void)
{
	while (true) {
		k_sem_take(&blink_sem, K_FOREVER);

		if (leds == NULL) {
			blink(BLINK_BLINK);
		} else {
			led_on(leds, blinker_led);
			k_sleep(K_MSEC(30));
			led_off(leds, blinker_led);
			k_sleep(K_MSEC(30));
		}
	}

	return 0;
}
