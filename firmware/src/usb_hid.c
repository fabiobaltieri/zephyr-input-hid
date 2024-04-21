#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>

#include "hid_kbd.h"
#include "hid_report_map.h"

LOG_MODULE_REGISTER(usb_hid_app, LOG_LEVEL_INF);

static const struct device *hid_dev;

static bool connected;

#if CONFIG_KBD_HID_NKRO
static struct hid_kbd_report_nkro_id report_kbd;
#else
static struct hid_kbd_report_id report_kbd;
struct hid_kbd_report_data data;
#endif

static void int_in_ready_cb(const struct device *dev)
{
	/* TODO: ep busy handling */
}

static void usb_hid_input_cb(struct input_event *evt)
{
	int ret;

#if CONFIG_KBD_HID_NKRO
	hid_kbd_input_process_nkro(&report_kbd.report, evt);
#else
	hid_kbd_input_process(&report_kbd.report, &data, evt);
#endif

	if (!connected) {
		return;
	}

	if (!input_queue_empty()) {
		return;
	}

	ret = hid_int_ep_write(hid_dev, (uint8_t *)&report_kbd, sizeof(report_kbd), NULL);
	if (ret) {
		LOG_ERR("HID write error, %d", ret);
		return;
	}
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET_OR_NULL(DT_NODELABEL(keymap)), usb_hid_input_cb);

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	LOG_DBG("usb hid status cb: %d", status);

	connected = (status == USB_DC_CONFIGURED);
}

static const struct hid_ops ops = {
        .int_in_ready = int_in_ready_cb,
};

static int usb_hid_setup(void)
{
	int ret;

	report_kbd.id = HID_REPORT_ID_KBD;

	hid_dev = device_get_binding("HID_0");
	if (hid_dev == NULL) {
		LOG_ERR("Cannot get USB HID Device");
		return 0;
	}

	usb_hid_register_device(hid_dev,
				hid_report_map, hid_report_map_len,
				&ops);

	usb_hid_init(hid_dev);

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	return 0;
}
SYS_INIT(usb_hid_setup, APPLICATION, 99);
