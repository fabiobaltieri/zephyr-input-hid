#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(leds, LOG_LEVEL_INF);

#define LEDSTRIP_NODE DT_NODELABEL(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(LEDSTRIP_NODE, chain_length)

static const struct device *led_strip = DEVICE_DT_GET(LEDSTRIP_NODE);

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

#define LEVEL 0x40

static const struct led_rgb colors[] = {
	RGB(LEVEL, 0x00, 0x00), /* red */
	RGB(0x00, LEVEL, 0x00), /* green */
	RGB(0x00, 0x00, LEVEL), /* blue */

	RGB(LEVEL, LEVEL, 0x00),
	RGB(0x00, LEVEL, LEVEL),
	RGB(LEVEL, 0x00, LEVEL),

	RGB(LEVEL, LEVEL, LEVEL),
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];
static struct led_rgb pixels_target[STRIP_NUM_PIXELS];

#define DELAY_TIME_MS 25
#define CYCLES 40
#define STEP (LEVEL / CYCLES)

static void step_pixel(struct led_rgb *value, struct led_rgb *target)
{
	if (value->r < target->r) {
		value->r = clamp(value->r + STEP, 0, LEVEL);
	} else {
		value->r = clamp(value->r - STEP, 0, LEVEL);
	}

	if (value->g < target->g) {
		value->g = clamp(value->g + STEP, 0, LEVEL);
	} else {
		value->g = clamp(value->g - STEP, 0, LEVEL);
	}

	if (value->b < target->b) {
		value->b = clamp(value->b + STEP, 0, LEVEL);
	} else {
		value->b = clamp(value->b - STEP, 0, LEVEL);
	}
}

static void leds_thread(void)
{
	int ret;

	if (!device_is_ready(led_strip)) {
		LOG_ERR("LED strip device %s is not ready", led_strip->name);
		return;
	}

	while (1) {

		for (uint8_t i = 0; i < STRIP_NUM_PIXELS; i++) {
			uint8_t color = sys_rand8_get() % ARRAY_SIZE(colors);

			memcpy(&pixels_target[i], &colors[color], sizeof(struct led_rgb));
		}

		for (uint8_t i = 0; i < CYCLES; i++) {
			for (uint8_t i = 0; i < STRIP_NUM_PIXELS; i++) {
				step_pixel(&pixels[i], &pixels_target[i]);
			}


			ret = led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
			if (ret) {
				LOG_ERR("led_strip_update_rgb error: %d", ret);
			}

			k_sleep(K_MSEC(DELAY_TIME_MS));
		}
	}

	return;
}

K_THREAD_DEFINE(leds, 1024, leds_thread, NULL, NULL, NULL, 1, 0, 0);
