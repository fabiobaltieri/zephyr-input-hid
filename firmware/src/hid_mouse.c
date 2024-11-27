#define DT_DRV_COMPAT hid_mouse

#include <stdint.h>
#include <hid_mouse.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#include "hid.h"

LOG_MODULE_REGISTER(hid_mouse, LOG_LEVEL_INF);

struct hid_mouse_report {
	uint8_t buttons;
	int16_t x;
	int16_t y;
	int16_t wheel;
} __packed;

struct hid_mouse_config {
	const struct device *hid_dev;
	uint8_t input_id;
	uint8_t feature_id;
};

struct hid_mouse_data {
	hid_mouse_feat_cb feat_cb;
};

static void hid_mouse_set_key_mouse(struct hid_mouse_report *report,
				    uint16_t code, uint32_t value)
{
	uint8_t bit;

	switch (code) {
	case INPUT_BTN_LEFT:
		bit = 0;
		break;
	case INPUT_BTN_RIGHT:
		bit = 1;
		break;
	case INPUT_BTN_MIDDLE:
		bit = 2;
		break;
	case INPUT_BTN_SIDE:
		bit = 3;
		break;
	case INPUT_BTN_EXTRA:
		bit = 4;
		break;
	case INPUT_BTN_FORWARD:
		bit = 5;
		break;
	case INPUT_BTN_BACK:
		bit = 6;
		break;
	case INPUT_BTN_TASK:
		bit = 7;
		break;
	default:
		return;
	}

	WRITE_BIT(report->buttons, bit, value);
}

static void hid_mouse_set_rel(struct hid_mouse_report *report,
			      uint16_t code, int32_t value)
{
	switch (code) {
	case INPUT_REL_X:
		report->x = CLAMP(report->x + value,
				  INT16_MIN, INT16_MAX);
		break;
	case INPUT_REL_Y:
		report->y = CLAMP(report->y + value,
				  INT16_MIN, INT16_MAX);
		break;
	case INPUT_REL_WHEEL:
		report->wheel = CLAMP(report->wheel + value,
				      INT16_MIN, INT16_MAX);
		break;
	default:
		return;
	}
}

static int hid_mouse_input_process(const struct device *dev,
				    uint8_t *buf, uint8_t len,
				    void *user_data)
{
	struct hid_mouse_report *report = (struct hid_mouse_report *)buf;
	struct input_event *evt = user_data;

	if (len < sizeof(*report)) {
		LOG_ERR("buffer too small %d < %d", len, sizeof(*report));
		return -EINVAL;
	}

	if (evt->type == INPUT_EV_KEY) {
		hid_mouse_set_key_mouse(report, evt->code, evt->value);
	} else if (evt->type == INPUT_EV_REL) {
		hid_mouse_set_rel(report, evt->code, evt->value);
	}

	return sizeof(*report);
}

static void hid_mouse_cb(struct input_event *evt, void *user_data)
{
	const struct device *dev = user_data;
        const struct hid_mouse_config *cfg = dev->config;

	hid_update_buffers(cfg->hid_dev, dev, cfg->input_id,
			   hid_mouse_input_process, evt);
}

static int hid_mouse_clear_rel(const struct device *dev,
			       uint8_t *buf, uint8_t len)
{
	struct hid_mouse_report *report = (struct hid_mouse_report *)buf;

	report->x = 0;
	report->y = 0;
	report->wheel = 0;

	return 0;
}

void hid_mouse_register_feat_cb(const struct device *dev, hid_mouse_feat_cb cb)
{
	struct hid_mouse_data *data = dev->data;

	data->feat_cb = cb;
}

static int hid_mouse_set_feature(const struct device *dev,
				uint8_t report_id, const uint8_t *buf, uint8_t len)
{
        const struct hid_mouse_config *cfg = dev->config;
	struct hid_mouse_data *data = dev->data;

	if (cfg->feature_id == 0) {
		LOG_ERR("feature-id not set");
		return -ENOSYS;
	}

	if (report_id != cfg->feature_id) {
		LOG_ERR("invalid report_id: %d", report_id);
		return -EINVAL;
	}

	if (data->feat_cb == NULL) {
		LOG_ERR("no feat callback set");
		return -ENOSYS;
	}

	return data->feat_cb(dev, report_id, buf, len);
}

static DEVICE_API(hid_input, hid_mouse_api) = {
	.clear_rel = hid_mouse_clear_rel,
	.set_feature = hid_mouse_set_feature,
};

#define HID_MOUSE_INPUT_CB(node_id, prop, idx, inst)					\
	INPUT_CALLBACK_DEFINE_NAMED(							\
			DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),		\
			hid_mouse_cb,							\
			(void *)DEVICE_DT_GET(node_id),					\
			hid_mouse_cb_##idx##_##inst);

#define HID_MOUSE_DEFINE(inst)								\
	DT_INST_FOREACH_PROP_ELEM_VARGS(inst, input, HID_MOUSE_INPUT_CB, inst)		\
											\
	static const struct hid_mouse_config hid_mouse_config_##inst = {		\
		.hid_dev = DEVICE_DT_GET(DT_INST_GPARENT(inst)),			\
		.input_id = DT_INST_PROP_BY_IDX(inst, input_id, 0),			\
		IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, feature_id), (			\
		.feature_id = DT_INST_PROP_BY_IDX(inst, feature_id, 0),			\
		))									\
	};										\
											\
	struct hid_mouse_data hid_mouse_data_##inst;					\
											\
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL,						\
			      &hid_mouse_data_##inst, &hid_mouse_config_##inst,		\
			      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, &hid_mouse_api);

DT_INST_FOREACH_STATUS_OKAY(HID_MOUSE_DEFINE)
