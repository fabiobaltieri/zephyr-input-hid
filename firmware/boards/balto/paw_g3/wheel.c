#include <hid_mouse.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_pat912x.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wheel, LOG_LEVEL_INF);

static const struct device *pat = DEVICE_DT_GET(DT_NODELABEL(pat));
static const struct device *hid_mouse = DEVICE_DT_GET_ONE(hid_mouse);

static int feat_cb(const struct device *dev,
		   uint8_t report_id, const uint8_t *buf, uint8_t len)
{
	if (len != 1) {
		LOG_WRN("invalid length %d", len);
		return -EINVAL;
	}

	if (!device_is_ready(pat)) {
		LOG_WRN("device %s not ready", pat->name);
		return -ENODEV;
	}

	if (buf[0] == 1) {
		LOG_INF("enabling hi-res on %s", pat->name);
		pat912x_set_resolution(pat, 1275, 1275);
	} else {
		LOG_INF("disabling hi-res on %s", pat->name);
		pat912x_set_resolution(pat, 50, 50);
	}

	return 0;
}

static int feat_cb_setup(void)
{
	hid_mouse_register_feat_cb(hid_mouse, feat_cb);
	return 0;
}
SYS_INIT(feat_cb_setup, APPLICATION, 99);
