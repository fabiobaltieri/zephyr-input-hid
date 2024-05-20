#define DT_DRV_COMPAT usb_hid

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>

#include "hid.h"

LOG_MODULE_REGISTER(usb_hid_app, LOG_LEVEL_INF);

#define DEVICE_DT_GET_COMMA(node_id) DEVICE_DT_GET(node_id),

static const struct device *hid_devs[] = {
	DT_FOREACH_STATUS_OKAY(hid, DEVICE_DT_GET_COMMA)
};

#define HID_DEVS_COUNT ARRAY_SIZE(hid_devs)

static const struct device *usb_hid_devs[HID_DEVS_COUNT];

static bool connected;
static bool suspended;
static bool busy;

static void int_in_ready_cb(const struct device *dev)
{
	busy = false;
	hid_output_notify(DEVICE_DT_INST_GET(0));
}

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	LOG_DBG("usb hid status cb: %d", status);

	connected = (status == USB_DC_CONFIGURED);
	suspended = (status == USB_DC_SUSPEND);
}

static const struct hid_ops ops = {
	.int_in_ready = int_in_ready_cb,
};

#define USB_HID_REPORT_BUF_SIZE 32

static void usb_hid_notify(const struct device *dev)
{
	int size;
	uint8_t buf[USB_HID_REPORT_BUF_SIZE + 1];
	int ret;

	if (busy) {
		return;
	}

	if (suspended) {
		usb_wakeup_request();
	}

	if (!connected) {
		return;
	}

	for (uint8_t i = 0; i < HID_DEVS_COUNT; i++) {
		const struct device *hid_dev = hid_devs[i];

		if (!hid_has_updates(hid_dev, dev)) {
			continue;
		}

		size = hid_get_report(hid_dev, dev, &buf[0], &buf[1], USB_HID_REPORT_BUF_SIZE);
		if (size < 0) {
			LOG_ERR("get_report error: %d", size);
			continue;
		}

		busy = true;

		ret = hid_int_ep_write(usb_hid_devs[i], buf, size + 1, NULL);
		if (ret) {
			LOG_ERR("HID write error, %d", ret);
			return;
		}
	}
}

static int usb_hid_out_init(const struct device *dev)
{
	int ret;

	for (uint8_t i = 0; i < HID_DEVS_COUNT; i++) {
		const struct device *hid_dev = hid_devs[i];
		const struct device *usb_hid_dev;
		const uint8_t *report = hid_report(hid_dev);
		uint16_t report_len = hid_report_len(hid_dev);
		char dev_name[8];

		snprintf(dev_name, sizeof(dev_name), "HID_%d", i);

		usb_hid_dev = device_get_binding(dev_name);
		if (usb_hid_dev == NULL) {
			LOG_ERR("Cannot get USB HID Device");
			return 0;
		}

		usb_hid_register_device(usb_hid_dev, report, report_len, &ops);

		usb_hid_init(usb_hid_dev);

		usb_hid_devs[i] = usb_hid_dev;
	}

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	return 0;
}

static const struct hid_output_api usb_hid_api = {
	.notify = usb_hid_notify,
};

DEVICE_DT_INST_DEFINE(0, usb_hid_out_init, NULL, NULL, NULL,
		      POST_KERNEL, 55, &usb_hid_api);
