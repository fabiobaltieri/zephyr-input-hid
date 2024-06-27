#define DT_DRV_COMPAT hid_kbd

#include <dt-bindings/hid.h>
#include <stdint.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_hid.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#include "hid.h"

LOG_MODULE_REGISTER(hid_kbd, LOG_LEVEL_INF);

#define HID_ROLLOVER_CODE 0x01

struct hid_kbd_report {
	uint8_t modifiers;
	uint8_t _reserved;
	uint8_t codes[HID_KBD_KEYS_CODES_SIZE];
} __packed;

struct hid_kbd_report_data {
	uint8_t bits[HID_KBD_KEYS_BITS_SIZE];
};

struct hid_kbd_report_nkro {
	uint8_t modifiers;
	uint8_t _reserved;
	uint8_t bits[HID_KBD_KEYS_BITS_SIZE];
} __packed;

struct hid_kbd_config {
	const struct device *hid_dev;
	uint8_t input_id;
	uint8_t output_id;
	struct hid_kbd_report_data *report_data;
};

static void hid_kbd_update_modifiers(uint8_t *modifiers, struct input_event *evt)
{
	uint8_t modifier;

	modifier = input_to_hid_modifier(evt->code);
	if (modifier != 0) {
		if (evt->value) {
			*modifiers |= modifier;
		} else {
			*modifiers &= ~modifier;
		}
	}
}

static int hid_kbd_update_bits(uint8_t bits[], struct input_event *evt)
{
	uint8_t off;
	uint8_t bit;
	int hid_code;

	hid_code = input_to_hid_code(evt->code);

	off = hid_code / 8;
	bit = hid_code % 8;

	if (off >= HID_KBD_KEYS_BITS_SIZE) {
		LOG_ERR("invalid hid code: %d", hid_code);
		return -EINVAL;
	}

	WRITE_BIT(bits[off], bit, evt->value);

	return 0;
}

static int hid_kbd_input_process(const struct device *dev,
				 uint8_t *buf, uint8_t len,
				 void *user_data)
{
	const struct hid_kbd_config *cfg = dev->config;
	struct hid_kbd_report_data *data = cfg->report_data;
	struct hid_kbd_report *report = (struct hid_kbd_report *)buf;
	struct input_event *evt = user_data;
	uint8_t bits_count;
	uint8_t code_off;
	uint8_t i;
	int ret;

	if (len < sizeof(*report)) {
		LOG_ERR("buffer too small %d < %d", len, sizeof(*report));
		return -EINVAL;
	}

	hid_kbd_update_modifiers(&report->modifiers, evt);

	ret = hid_kbd_update_bits(data->bits, evt);
	if (ret < 0) {
		return -EINVAL;
	}

	bits_count = 0;
	for (i = 0; i < ARRAY_SIZE(data->bits); i++) {
		bits_count += POPCOUNT(data->bits[i]);
	}

	if (bits_count > HID_KBD_KEYS_CODES_SIZE) {
		memset(report->codes, HID_ROLLOVER_CODE, sizeof(report->codes));
		return sizeof(*report);
	}

	code_off = 0;
	memset(report->codes, 0, sizeof(report->codes));
	for (i = 0; i < HID_KBD_KEYS_BITS_SIZE * 8; i++) {
		uint8_t off = i / 8;
		uint8_t bit = i % 8;

		if ((data->bits[off] & BIT(bit)) != 0x00) {
			report->codes[code_off] = i;
			code_off++;
		}
	}

	return sizeof(*report);
}

static int hid_kbd_input_process_nkro(const struct device *dev,
				       uint8_t *buf, uint8_t len,
				       void *user_data)
{
	struct hid_kbd_report_nkro *report = (struct hid_kbd_report_nkro *)buf;
	struct input_event *evt = user_data;
	int ret;

	if (len < sizeof(*report)) {
		LOG_ERR("buffer too small %d < %d", len, sizeof(*report));
		return -EINVAL;
	}

	hid_kbd_update_modifiers(&report->modifiers, evt);

	ret = hid_kbd_update_bits(report->bits, evt);
	if (ret < 0) {
		return -EINVAL;
	}

	return sizeof(*report);
}

static void hid_kbd_cb(const struct device *dev, struct input_event *evt)
{
	const struct hid_kbd_config *cfg = dev->config;

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	if (cfg->report_data) {
		hid_update_buffers(cfg->hid_dev, dev, cfg->input_id,
				   hid_kbd_input_process, evt);
	} else {
		hid_update_buffers(cfg->hid_dev, dev, cfg->input_id,
				   hid_kbd_input_process_nkro, evt);
	}
}

static int hid_kbd_out_report(const struct device *dev,
			      uint8_t report_id, const uint8_t *buf, uint8_t len)
{
	const struct hid_kbd_config *cfg = dev->config;

	if (cfg->output_id == 0) {
		LOG_ERR("output-id not set");
		return -ENOSYS;
	}

	if (report_id != cfg->output_id) {
		LOG_ERR("invalid report_id: %d", report_id);
		return -EINVAL;
	}

	LOG_INF("%d %d", report_id, len);

	return 0;
}

static const struct hid_input_api hid_kbd_api = {
	.out_report = hid_kbd_out_report,
};

#define HID_KBD_INPUT_CB(node_id, prop, idx, inst) \
	static void hid_kbd_cb_##inst##_##idx(struct input_event *evt)			\
	{										\
		hid_kbd_cb(DEVICE_DT_GET(node_id), evt);				\
	}										\
	INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),	\
			      hid_kbd_cb_##inst##_##idx);

#define HID_KBD_DEFINE(inst)								\
	DT_INST_FOREACH_PROP_ELEM_VARGS(inst, input, HID_KBD_INPUT_CB, inst)		\
											\
	COND_CODE_1(DT_INST_PROP(inst, nkro), (), (					\
	static struct hid_kbd_report_data hid_kbd_report_data_##inst;			\
	))										\
											\
	static const struct hid_kbd_config hid_kbd_config_##inst = {			\
		.hid_dev = DEVICE_DT_GET(DT_INST_GPARENT(inst)),			\
		.input_id = DT_INST_PROP_BY_IDX(inst, input_id, 0),			\
		IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, output_id), (			\
		.output_id = DT_INST_PROP_BY_IDX(inst, output_id, 0),			\
		))									\
		COND_CODE_1(DT_INST_PROP(inst, nkro), (), (				\
		.report_data = &hid_kbd_report_data_##inst,				\
		))									\
	};										\
											\
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL, &hid_kbd_config_##inst,		\
			      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, &hid_kbd_api);

DT_INST_FOREACH_STATUS_OKAY(HID_KBD_DEFINE)
