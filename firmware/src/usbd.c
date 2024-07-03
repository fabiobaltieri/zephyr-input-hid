#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/input/input.h>

LOG_MODULE_REGISTER(usbd_app, LOG_LEVEL_INF);

#define USBD_VID 0x2fe3
#define USBD_PID 0x0007
#define USBD_MAX_POWER 250

USBD_DEVICE_DEFINE(app_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   USBD_VID, USBD_PID);

USBD_DESC_LANG_DEFINE(app_usb_lang);
USBD_DESC_MANUFACTURER_DEFINE(app_usb_mfr, CONFIG_APP_MANUFACTURER_NAME);
USBD_DESC_PRODUCT_DEFINE(app_usb_product, CONFIG_APP_DEVICE_NAME);
USBD_DESC_SERIAL_NUMBER_DEFINE(app_usb_sn);

static const uint8_t attributes = USB_SCD_REMOTE_WAKEUP;

USBD_CONFIGURATION_DEFINE(app_usb_fs_config, attributes, USBD_MAX_POWER);

USBD_CONFIGURATION_DEFINE(app_usb_hs_config, attributes, USBD_MAX_POWER);

static void fix_code_triple(struct usbd_context *uds_ctx, const enum usbd_speed speed)
{
	if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS)) {
		usbd_device_set_code_triple(uds_ctx, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
	}
}

struct usbd_context *usbd_init_device(void)
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

		fix_code_triple(&app_usbd, USBD_SPEED_HS);
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

	fix_code_triple(&app_usbd, USBD_SPEED_FS);

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

	ret = usbd_enable(usbd);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	return 0;
}
SYS_INIT(app_usbd_enable, APPLICATION, 0);

#if 0
static void input_resume_cb(struct input_event *evt)
{
	if (!evt->sync) {
		return;
	}

	if (usbd_is_suspended(&app_usbd)) {
		usbd_wakeup_request(&app_usbd);
	}

}
INPUT_CALLBACK_DEFINE(NULL, input_resume_cb);
#endif
