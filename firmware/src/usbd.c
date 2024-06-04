#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>

LOG_MODULE_REGISTER(usbd_app, LOG_LEVEL_INF);

#include <zephyr/usb/bos.h>

#define USBD_VID 0x2fe3
#define USBD_PID 0x0007
#define USBD_MAX_POWER 250

USBD_DEVICE_DEFINE(sample_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   USBD_VID, USBD_PID);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, CONFIG_APP_MANUFACTURER_NAME);
USBD_DESC_PRODUCT_DEFINE(sample_product, CONFIG_APP_DEVICE_NAME);
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn);

static const uint8_t attributes = USB_SCD_REMOTE_WAKEUP;

USBD_CONFIGURATION_DEFINE(sample_fs_config, attributes, USBD_MAX_POWER);

USBD_CONFIGURATION_DEFINE(sample_hs_config, attributes, USBD_MAX_POWER);

static int register_fs_classes(struct usbd_context *uds_ctx)
{
	int err;

	STRUCT_SECTION_FOREACH_ALTERNATE(usbd_class_fs, usbd_class_node, c_nd) {
		err = usbd_register_class(uds_ctx, c_nd->c_data->name, USBD_SPEED_FS, 1);
		if (err) {
			LOG_ERR("Failed to register FS %s (%d)", c_nd->c_data->name, err);
			return err;
		}
	}

	return 0;
}

static int register_hs_classes(struct usbd_context *uds_ctx)
{
	int err;

	STRUCT_SECTION_FOREACH_ALTERNATE(usbd_class_hs, usbd_class_node, c_nd) {
		err = usbd_register_class(uds_ctx, c_nd->c_data->name, USBD_SPEED_HS, 1);
		if (err) {
			LOG_ERR("Failed to register HS %s (%d)", c_nd->c_data->name, err);
			return err;
		}
	}

	return 0;
}

static int sample_add_configuration(struct usbd_context *uds_ctx,
				    const enum usbd_speed speed,
				    struct usbd_config_node *config)
{
	int err;

	err = usbd_add_configuration(uds_ctx, speed, config);
	if (err) {
		LOG_ERR("Failed to add configuration (%d)", err);
		return err;
	}

	if (speed == USBD_SPEED_FS) {
		err = register_fs_classes(uds_ctx);
	} else if (speed == USBD_SPEED_HS) {
		err = register_hs_classes(uds_ctx);
	}

	if (err) {
		return err;
	}

	if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS)) {
		usbd_device_set_code_triple(uds_ctx, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
	}

	return 0;
}

struct usbd_context *usbd_init_device(void)
{
	int err;

	err = usbd_add_descriptor(&sample_usbd, &sample_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return NULL;
	}

	if (usbd_caps_speed(&sample_usbd) == USBD_SPEED_HS) {
		err = sample_add_configuration(&sample_usbd, USBD_SPEED_HS, &sample_hs_config);
		if (err) {
			LOG_ERR("Failed to add High-Speed configuration");
			return NULL;
		}
	}

	err = sample_add_configuration(&sample_usbd, USBD_SPEED_FS, &sample_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration");
		return NULL;
	}

	err = usbd_init(&sample_usbd);
	if (err) {
		LOG_ERR("Failed to initialize device support");
		return NULL;
	}

	return &sample_usbd;
}

static int global_usbd_enable(void)
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
SYS_INIT(global_usbd_enable, APPLICATION, 0);
