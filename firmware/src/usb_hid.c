#define DT_DRV_COMPAT usb_hid

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>

#include "hid.h"

LOG_MODULE_REGISTER(usb_hid_app, LOG_LEVEL_INF);

struct usb_hid_config {
	const struct device *hid_dev;
	uint8_t idx;
};

struct usb_hid_data {
	const struct device *usb_hid_dev;
	bool busy;
};

static bool connected;
static bool suspended;

#define DEVICE_DT_GET_COMMA(n) DEVICE_DT_INST_GET(n),

static const struct device *usb_hid_devs[] = {
	DT_INST_FOREACH_STATUS_OKAY(DEVICE_DT_GET_COMMA)
};

#define USB_HID_DEV_COUNT ARRAY_SIZE(usb_hid_devs)

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	LOG_DBG("usb hid status cb: %d", status);

	connected = (status == USB_DC_CONFIGURED);
	suspended = (status == USB_DC_SUSPEND);
}

static int global_usb_enable(void)
{
	int ret;

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	return 0;
}
SYS_INIT(global_usb_enable, APPLICATION, 0);

static const struct device *find_hid_dev(const struct device *dev)
{
	struct usb_hid_data *data;
	uint8_t i;

	for (i = 0; i < USB_HID_DEV_COUNT; i++) {
		data = usb_hid_devs[i]->data;
		if (data->usb_hid_dev == dev) {
			return usb_hid_devs[i];
		}
	}

	LOG_ERR("unknown hid device for %s", dev->name);

	return NULL;
}

static void int_in_ready_cb(const struct device *dev)
{
	const struct device *hid_dev;
	struct usb_hid_data *data;

	hid_dev = find_hid_dev(dev);
	if (hid_dev == NULL) {
		return;
	}

	data = hid_dev->data;

	data->busy = false;

	hid_output_notify(hid_dev);
}

#define USB_HID_REPORT_BUF_SIZE 32

static void int_out_ready_cb(const struct device *dev)
{
	const struct device *hid_dev;
	const struct usb_hid_config *cfg;
	char buf[USB_HID_REPORT_BUF_SIZE];
	uint32_t len;
	int ret;

	hid_dev = find_hid_dev(dev);
	if (hid_dev == NULL) {
		return;
	}

	cfg = hid_dev->config;

	ret = hid_int_ep_read(dev, buf, sizeof(buf), &len);
	if (ret) {
		LOG_ERR("HID read error, %d", ret);
		return;
	}

	if (len < 1) {
		LOG_ERR("short write");
		return;
	}

	hid_out_report(cfg->hid_dev, buf[0], &buf[1], len - 1);
}

static const struct hid_ops usb_hid_ops = {
	.int_in_ready = int_in_ready_cb,
	.int_out_ready = int_out_ready_cb,
};

static void usb_hid_notify(const struct device *dev)
{
	const struct usb_hid_config *cfg = dev->config;
	struct usb_hid_data *data = dev->data;
	int size;
	uint8_t buf[USB_HID_REPORT_BUF_SIZE + 1];
	int ret;

	if (data->busy) {
		return;
	}

	if (suspended) {
		usb_wakeup_request();
	}

	size = hid_get_report(cfg->hid_dev, dev, &buf[0], &buf[1], USB_HID_REPORT_BUF_SIZE);
	if (size == -EAGAIN) {
		return;
	} else if (size < 0) {
		LOG_ERR("get_report error: %d", size);
		return;
	}

	if (!connected) {
		return;
	}

	data->busy = true;

	ret = hid_int_ep_write(data->usb_hid_dev, buf, size + 1, NULL);
	if (ret) {
		LOG_ERR("HID write error, %d", ret);
		return;
	}
}

static int usb_hid_out_init(const struct device *dev)
{
	const struct usb_hid_config *cfg = dev->config;
	struct usb_hid_data *data = dev->data;
	const uint8_t *report = hid_report(cfg->hid_dev);
	uint16_t report_len = hid_report_len(cfg->hid_dev);
	const struct device *usb_hid_dev;
	char dev_name[8];

	snprintf(dev_name, sizeof(dev_name), "HID_%d", cfg->idx);

	usb_hid_dev = device_get_binding(dev_name);
	if (usb_hid_dev == NULL) {
		LOG_ERR("Cannot get USB HID Device");
		return 0;
	}

	usb_hid_register_device(usb_hid_dev, report, report_len, &usb_hid_ops);

	usb_hid_init(usb_hid_dev);

	data->usb_hid_dev = usb_hid_dev;

	return 0;
}

static const struct hid_output_api usb_hid_api = {
	.notify = usb_hid_notify,
};

#define USB_HID_INIT(n)							\
	static const struct usb_hid_config usb_hid_cfg_##n = {		\
		.hid_dev = DEVICE_DT_GET(DT_INST_GPARENT(n)),		\
		.idx = n,						\
	};								\
									\
	static struct usb_hid_data usb_hid_data_##n;			\
									\
	DEVICE_DT_INST_DEFINE(n, usb_hid_out_init, NULL,		\
			      &usb_hid_data_##n, &usb_hid_cfg_##n,	\
			      POST_KERNEL, 55, &usb_hid_api);

DT_INST_FOREACH_STATUS_OKAY(USB_HID_INIT)
