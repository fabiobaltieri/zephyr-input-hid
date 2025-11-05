#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led_strip.h>

LOG_MODULE_REGISTER(leds, LOG_LEVEL_INF);

#define STRIP_NODE              DT_NODELABEL(led_strip)
#define STRIP_NUM_PIXELS        DT_PROP(DT_NODELABEL(led_strip), chain_length)

#define DELAY_TIME K_MSEC(200)
#define CONFIG_SAMPLE_LED_BRIGHTNESS 0xff

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
	RGB(CONFIG_SAMPLE_LED_BRIGHTNESS, 0x00, 0x00), /* red */
	RGB(0x00, CONFIG_SAMPLE_LED_BRIGHTNESS, 0x00), /* green */
	RGB(0x00, 0x00, CONFIG_SAMPLE_LED_BRIGHTNESS), /* blue */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

static void leds_thread(void)
{
	size_t color = 0;
	int ret;

	if (device_is_ready(strip)) {
		LOG_INF("Found LED strip device %s", strip->name);
	} else {
		LOG_ERR("LED strip device %s is not ready", strip->name);
		return;
	}

	LOG_INF("Displaying pattern on strip");
	while (1) {
		for (size_t cursor = 0; cursor < ARRAY_SIZE(pixels); cursor++) {
			memset(&pixels, 0x00, sizeof(pixels));
			memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));

			ret = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
			if (ret) {
				LOG_ERR("couldn't update strip: %d", ret);
			}

			k_sleep(DELAY_TIME);
		}

		color = (color + 1) % ARRAY_SIZE(colors);
	}

	return;
}

K_THREAD_DEFINE(leds, 1024, leds_thread, NULL, NULL, NULL, 14, 0, 0);
