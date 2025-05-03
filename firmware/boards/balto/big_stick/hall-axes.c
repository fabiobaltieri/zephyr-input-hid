#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hall_axes, LOG_LEVEL_INF);

#define TMAG_UPDATE_RATE_MSEC 33

static struct {
	const struct device *dev;
	const uint16_t axis;
	const uint16_t min;
	const uint16_t max;
	const bool invert;
	uint16_t last_raw_val;
	uint16_t last_val;
} devs[] = {
	{
		.dev = DEVICE_DT_GET(DT_NODELABEL(tmag0)),
		.axis = INPUT_ABS_X,
		.min = 754,
		.max = 1072,
	},
	{
		.dev = DEVICE_DT_GET(DT_NODELABEL(tmag1)),
		.axis = INPUT_ABS_Y,
		.min = 1000,
		.max = 690,
	},
};

static void tmag_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(tmag_dwork, tmag_handler);

static void tmag_handler(struct k_work *work)
{
	for (uint8_t i = 0; i < ARRAY_SIZE(devs); i++) {
		const struct device *dev = devs[i].dev;
		struct sensor_value val;
		int ret;

		ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_ROTATION);
		if (ret < 0) {
			LOG_ERR("sample_fetch error: %d", ret);
			return;
		}

		ret = sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &val);
		if (ret < 0) {
			LOG_ERR("channel_get error: %d", ret);
			return;
		}

		int32_t deg = val.val1 * 10 + val.val2 / 1000000;
		int32_t range = devs[i].max - devs[i].min;
		int32_t out = CLAMP((deg - devs[i].min) * 0xff / range, 0, 0xff);

		if (devs[i].last_val != out) {
			input_report_abs(dev, devs[i].axis, out, true, K_FOREVER);
		}
		devs[i].last_val = out;

		LOG_DBG("%s: %5d %3d", dev->name, deg, out);
	}

	k_work_schedule(&tmag_dwork, K_MSEC(TMAG_UPDATE_RATE_MSEC));
}

static int tmag_init(void)
{
	for (uint8_t i = 0; i < ARRAY_SIZE(devs); i++) {
		if (!device_is_ready(devs[i].dev)) {
			LOG_ERR("%s device is not ready", devs[i].dev->name);
			return -ENODEV;
		}
	}

	tmag_handler(&tmag_dwork.work);

	return 0;
}
SYS_INIT(tmag_init, APPLICATION, 99);
