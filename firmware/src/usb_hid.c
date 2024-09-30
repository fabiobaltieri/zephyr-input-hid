#define DT_DRV_COMPAT usb_hid

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/usb/usbd.h>

#include "hid.h"

LOG_MODULE_REGISTER(usb_hid_app, LOG_LEVEL_INF);

#define USB_HID_REPORT_BUF_SIZE 32

struct usb_hid_config {
	const struct device *hid_dev;
	const struct device *usb_hid_dev;
};

struct usb_hid_data {
	char buf[USB_HID_REPORT_BUF_SIZE];
	bool busy;
	bool connected;
	bool suspended;
};

#define DEVICE_DT_GET_COMMA(n) DEVICE_DT_INST_GET(n),

static const struct device *usb_hid_devs[] = {
	DT_INST_FOREACH_STATUS_OKAY(DEVICE_DT_GET_COMMA)
};

#define USB_HID_DEV_COUNT ARRAY_SIZE(usb_hid_devs)

static const struct device *find_hid_dev(const struct device *dev)
{
	const struct usb_hid_config *cfg;
	uint8_t i;

	for (i = 0; i < USB_HID_DEV_COUNT; i++) {
		cfg = usb_hid_devs[i]->config;
		if (cfg->usb_hid_dev == dev) {
			return usb_hid_devs[i];
		}
	}

	LOG_ERR("unknown hid device for %s", dev->name);

	return NULL;
}

static void iface_ready(const struct device *dev, const bool ready)
{
	const struct device *hid_dev;
	struct usb_hid_data *data;

	hid_dev = find_hid_dev(dev);
	if (hid_dev == NULL) {
		return;
	}

	data = hid_dev->data;

	data->connected = ready;
}

static void input_report_done(const struct device *dev)
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

static int get_report(const struct device *dev,
		      const uint8_t type, const uint8_t id,
		      const uint16_t len, uint8_t *const buf)
{
	const struct device *hid_dev;
	const struct usb_hid_config *cfg;
	int size;

	if (type != HID_REPORT_TYPE_INPUT) {
		return 0;
	}

	hid_dev = find_hid_dev(dev);
	if (hid_dev == NULL) {
		return -EIO;
	}

	cfg = hid_dev->config;

	size = hid_get_report_id(cfg->hid_dev, hid_dev, id, buf, len);
	if (size < 0) {
		LOG_WRN("hid_get_report error: %d", size);
		return 0;
	}

	return size;
}

static void output_report(const struct device *dev,
			  const uint16_t len, const uint8_t *const buf)
{
	const struct device *hid_dev;
	const struct usb_hid_config *cfg;

	hid_dev = find_hid_dev(dev);
	if (hid_dev == NULL) {
		return;
	}

	cfg = hid_dev->config;

	if (len < 1) {
		LOG_ERR("short write");
		return;
	}

	hid_out_report(cfg->hid_dev, buf[0], &buf[1], len - 1);
}

static int set_report(const struct device *dev,
		      const uint8_t type, const uint8_t id, const uint16_t len,
		      const uint8_t *const buf)
{
	const struct device *hid_dev;
	const struct usb_hid_config *cfg;

	if (len < 1) {
		LOG_ERR("short write");
		return -EIO;
	}

	if (id != buf[0]) {
		LOG_ERR("set report id error %d != %d", id, buf[0]);
		return -EIO;
	}

	hid_dev = find_hid_dev(dev);
	if (hid_dev == NULL) {
		return -EIO;
	}

	cfg = hid_dev->config;

	if (type == HID_REPORT_TYPE_OUTPUT) {
		hid_out_report(cfg->hid_dev, id, &buf[1], len - 1);
	} else if (type == HID_REPORT_TYPE_FEATURE) {
		return hid_set_feature(cfg->hid_dev, id, &buf[1], len - 1);
	} else {
		LOG_WRN("unsupported report type: %d", type);
		return -ENOTSUP;
	}

	return 0;
}

static void set_protocol(const struct device *dev, const uint8_t proto)
{
	LOG_WRN("%s unimplemented", __func__);
}

static const struct hid_device_ops usb_hid_ops = {
	.iface_ready = iface_ready,
	.input_report_done = input_report_done,
	.get_report = get_report,
	.set_report = set_report,
	.set_protocol = set_protocol,
	.output_report = output_report,
};

static void usb_hid_notify(const struct device *dev)
{
	const struct usb_hid_config *cfg = dev->config;
	struct usb_hid_data *data = dev->data;
	int size;
	int ret;

	if (data->busy) {
		return;
	}

	size = hid_get_report(cfg->hid_dev, dev, &data->buf[0], &data->buf[1], USB_HID_REPORT_BUF_SIZE - 1);
	if (size == -EAGAIN) {
		return;
	} else if (size < 0) {
		LOG_ERR("get_report error: %d", size);
		return;
	}

	if (!data->connected) {
		return;
	}

	if (data->suspended) {
		return;
	}

	data->busy = true;

	ret = hid_device_submit_report(cfg->usb_hid_dev, size + 1, data->buf);
	if (ret) {
		LOG_ERR("HID write error, %d", ret);
		return;
	}
}

static int usb_hid_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	struct usb_hid_data *data = dev->data;

	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
		data->suspended = true;
		break;
	case PM_DEVICE_ACTION_RESUME:
		data->suspended = false;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int usb_hid_out_init(const struct device *dev)
{
	const struct usb_hid_config *cfg = dev->config;
	const uint8_t *report = hid_report(cfg->hid_dev);
	uint16_t report_len = hid_report_len(cfg->hid_dev);

	hid_device_register(cfg->usb_hid_dev, report, report_len, &usb_hid_ops);

	usb_hid_init(cfg->usb_hid_dev);

	return 0;
}

static const struct hid_output_api usb_hid_api = {
	.notify = usb_hid_notify,
};

#define USB_HID_INIT(n)								\
	static const struct usb_hid_config usb_hid_cfg_##n = {			\
		.hid_dev = DEVICE_DT_GET(DT_INST_GPARENT(n)),			\
		.usb_hid_dev = DEVICE_DT_GET(DT_INST_PHANDLE(n, usb_hid_dev)),	\
	};									\
										\
	static struct usb_hid_data usb_hid_data_##n;				\
										\
	PM_DEVICE_DT_INST_DEFINE(n, usb_hid_pm_action);				\
										\
	DEVICE_DT_INST_DEFINE(n, usb_hid_out_init, PM_DEVICE_DT_INST_GET(n),	\
			      &usb_hid_data_##n, &usb_hid_cfg_##n,		\
			      POST_KERNEL, 55, &usb_hid_api);

DT_INST_FOREACH_STATUS_OKAY(USB_HID_INIT)
