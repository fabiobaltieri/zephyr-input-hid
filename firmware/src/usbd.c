#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/input/input.h>

#include "event.h"

LOG_MODULE_REGISTER(usbd_app, LOG_LEVEL_INF);

USBD_DEVICE_DEFINE(app_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_APP_USB_VID,
		   CONFIG_APP_USB_PID);

USBD_DESC_LANG_DEFINE(app_usb_lang);
USBD_DESC_MANUFACTURER_DEFINE(app_usb_mfr, CONFIG_APP_MANUFACTURER_NAME);
USBD_DESC_PRODUCT_DEFINE(app_usb_product, CONFIG_APP_DEVICE_NAME);
USBD_DESC_SERIAL_NUMBER_DEFINE(app_usb_sn);

static const uint8_t attributes = (
		IS_ENABLED(CONFIG_APP_USB_REMOTE_WAKEUP) ? USB_SCD_REMOTE_WAKEUP : 0);

USBD_CONFIGURATION_DEFINE(app_usb_fs_config, attributes, CONFIG_APP_USB_MAX_POWER, NULL);

USBD_CONFIGURATION_DEFINE(app_usb_hs_config, attributes, CONFIG_APP_USB_MAX_POWER, NULL);

static void usbd_msg_cb(struct usbd_context *const usbd_ctx,
			const struct usbd_msg *const msg)
{
	static bool vbus_ready;
	static bool connected;
	int ret;

	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (!usbd_can_detect_vbus(usbd_ctx)) {
		return;
	}

	switch (msg->type) {
	case USBD_MSG_VBUS_READY:
		vbus_ready = true;

		ret = usbd_enable(usbd_ctx);
		if (ret) {
			LOG_ERR("Failed to enable device support");
		}

		break;
	case USBD_MSG_VBUS_REMOVED:
		vbus_ready = false;

		ret = usbd_disable(usbd_ctx);
		if (ret) {
			LOG_ERR("Failed to disable device support");
		}

		break;
	case USBD_MSG_RESUME:
		if (vbus_ready && !connected) {
			event(EVENT_USB_CONNECTED);
			connected = true;
		}

		break;
	case USBD_MSG_SUSPEND:
		if (connected) {
			event(EVENT_USB_DISCONNECTED);
			connected = false;
		}

		break;
	default:
		break;
	}
}

static struct usbd_context *usbd_init_device(void)
{
	int err;

	err = usbd_add_descriptor(&app_usbd, &app_usb_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&app_usbd, &app_usb_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&app_usbd, &app_usb_product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&app_usbd, &app_usb_sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return NULL;
	}

	if (usbd_caps_speed(&app_usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&app_usbd, USBD_SPEED_HS, &app_usb_hs_config);
		if (err) {
			LOG_ERR("Failed to add High-Speed configuration");
			return NULL;
		}

		err = usbd_register_all_classes(&app_usbd, USBD_SPEED_HS, 1);
		if (err) {
			LOG_ERR("Failed to add register classes");
			return NULL;
		}

		usbd_device_set_code_triple(&app_usbd, USBD_SPEED_HS, 0, 0, 0);
	}

	err = usbd_add_configuration(&app_usbd, USBD_SPEED_FS, &app_usb_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration");
		return NULL;
	}

	err = usbd_register_all_classes(&app_usbd, USBD_SPEED_FS, 1);
	if (err) {
		LOG_ERR("Failed to add register classes");
		return NULL;
	}

	usbd_device_set_code_triple(&app_usbd, USBD_SPEED_FS, 0, 0, 0);

	err = usbd_msg_register_cb(&app_usbd, usbd_msg_cb);
	if (err) {
		LOG_ERR("Failed to register message callback");
		return NULL;
	}

	err = usbd_init(&app_usbd);
	if (err) {
		LOG_ERR("Failed to initialize device support");
		return NULL;
	}

	return &app_usbd;
}

static int app_usbd_enable(void)
{
	struct usbd_context *usbd;
	int ret;

	usbd = usbd_init_device();
	if (usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (usbd_can_detect_vbus(usbd)) {
		return 0;
	}

	ret = usbd_enable(usbd);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	event(EVENT_USB_CONNECTED);

	return 0;
}
SYS_INIT(app_usbd_enable, APPLICATION, 0);

#if CONFIG_DT_HAS_USB_WAKEUP_ENABLED
#define USB_WAKEUP_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(usb_wakeup)

static const uint16_t input_wakeup_codes[] = DT_PROP(USB_WAKEUP_NODE, key_codes);

static void input_wakeup_cb(struct input_event *evt, void *user_data)
{
	uint8_t i;

	if (!usbd_is_suspended(&app_usbd)) {
		return;
	}

	if (evt->type != INPUT_EV_KEY ||
	    !evt->sync ||
	    evt->value == 0) {
		return;
	}

	for (i = 0; i < ARRAY_SIZE(input_wakeup_codes); i++) {
		if (input_wakeup_codes[i] == evt->code) {
			break;
		}
	}
	if (i == ARRAY_SIZE(input_wakeup_codes)) {
		return;
	}

	usbd_wakeup_request(&app_usbd);
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET_OR_NULL(DT_PHANDLE(USB_WAKEUP_NODE, input)),
		      input_wakeup_cb, NULL);
#endif
