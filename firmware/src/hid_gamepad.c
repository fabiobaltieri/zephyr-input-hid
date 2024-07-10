#define DT_DRV_COMPAT hid_gamepad

#include <stdint.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#include "hid.h"

LOG_MODULE_REGISTER(hid_gamepad, LOG_LEVEL_INF);

struct hid_gamepad_report {
	uint8_t hat;
	uint16_t buttons;
	uint8_t x;
	uint8_t y;
	uint8_t rx;
	uint8_t ry;
} __packed;

struct hid_gamepad_config {
	const struct device *hid_dev;
	uint8_t input_id;
};

#define HAT_UP 0
#define HAT_DOWN 1
#define HAT_LEFT 2
#define HAT_RIGHT 3

struct code_to_bit_map {
	uint16_t code;
	uint8_t bit;
};

static const struct code_to_bit_map hat_map[] = {
	{INPUT_BTN_DPAD_UP, HAT_UP},
	{INPUT_BTN_DPAD_DOWN, HAT_DOWN},
	{INPUT_BTN_DPAD_LEFT, HAT_LEFT},
	{INPUT_BTN_DPAD_RIGHT, HAT_RIGHT},
};

static int code_to_bit(const struct code_to_bit_map *map, int map_size,
		       uint16_t code)
{
	int i;

	for (i = 0; i < map_size; i++) {
		if (map[i].code == code) {
			return map[i].bit;
		}
	}
	return -1;
}

static void hid_gamepad_set_hat(struct hid_gamepad_report *report,
				uint16_t code, uint32_t value)
{
	static uint8_t hat_bits;
	int bit;

	bit = code_to_bit(hat_map, ARRAY_SIZE(hat_map), code);
	if (bit < 0) {
		return;
	}

	WRITE_BIT(hat_bits, bit, value);

	switch (hat_bits) {
	case BIT(HAT_UP):
		report->hat = 0;
		break;
	case BIT(HAT_UP) | BIT(HAT_RIGHT):
		report->hat = 1;
		break;
	case BIT(HAT_RIGHT):
		report->hat = 2;
		break;
	case BIT(HAT_RIGHT) | BIT(HAT_DOWN):
		report->hat = 3;
		break;
	case BIT(HAT_DOWN):
		report->hat = 4;
		break;
	case BIT(HAT_DOWN) | BIT(HAT_LEFT):
		report->hat = 5;
		break;
	case BIT(HAT_LEFT):
		report->hat = 6;
		break;
	case BIT(HAT_LEFT) | BIT(HAT_UP):
		report->hat = 7;
		break;
	default:
		report->hat = 8;
	}
}

static const struct code_to_bit_map button_map[] = {
	{INPUT_BTN_SOUTH, 0},
	{INPUT_BTN_EAST, 1},
	{INPUT_BTN_NORTH, 2},
	{INPUT_BTN_WEST, 3},
	{INPUT_BTN_TL, 4},
	{INPUT_BTN_TR, 5},
	{INPUT_BTN_SELECT, 6},
	{INPUT_BTN_START, 7},
	{INPUT_BTN_MODE, 8},
	{INPUT_BTN_THUMBL, 9},
	{INPUT_BTN_THUMBR, 10},
	{INPUT_BTN_TL2, 11},
	{INPUT_BTN_TR2, 12},
};

static void hid_gamepad_set_key(struct hid_gamepad_report *report,
				uint16_t code, uint32_t value)
{
	int bit;

	bit = code_to_bit(button_map, ARRAY_SIZE(button_map), code);
	if (bit < 0) {
		return;
	}

	WRITE_BIT(report->buttons, bit, value);
}

static void hid_gamepad_set_abs(struct hid_gamepad_report *report,
				uint16_t code, uint32_t value)
{
	switch (code) {
	case INPUT_ABS_X:
		report->x = CLAMP(value, 0, UINT8_MAX);
		break;
	case INPUT_ABS_Y:
		report->y = CLAMP(value, 0, UINT8_MAX);
		break;
	case INPUT_ABS_RX:
		report->rx = CLAMP(value, 0, UINT8_MAX);
		break;
	case INPUT_ABS_RY:
		report->ry = CLAMP(value, 0, UINT8_MAX);
		break;
	default:
		return;
	}
}

static int hid_gamepad_input_process(const struct device *dev,
				     uint8_t *buf, uint8_t len,
				     void *user_data)
{
	struct hid_gamepad_report *report = (struct hid_gamepad_report *)buf;
	struct input_event *evt = user_data;

	if (len < sizeof(*report)) {
		LOG_ERR("buffer too small %d < %d", len, sizeof(*report));
		return -EINVAL;
	}

	if (evt->type == INPUT_EV_KEY) {
		hid_gamepad_set_hat(report, evt->code, evt->value);
		hid_gamepad_set_key(report, evt->code, evt->value);
	} else if (evt->type == INPUT_EV_ABS) {
		hid_gamepad_set_abs(report, evt->code, evt->value);
	}

	return sizeof(*report);
}

static void hid_gamepad_cb(const struct device *dev, struct input_event *evt)
{
	const struct hid_gamepad_config *cfg = dev->config;

	hid_update_buffers(cfg->hid_dev, dev, cfg->input_id,
			   hid_gamepad_input_process, evt);
}

#define HID_GAMEPAD_INPUT_CB(node_id, prop, idx, inst) \
	static void hid_gamepad_cb_##inst##_##idx(struct input_event *evt)		\
	{										\
		hid_gamepad_cb(DEVICE_DT_GET(node_id), evt);				\
	}										\
	INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),	\
			      hid_gamepad_cb_##inst##_##idx);

#define HID_GAMEPAD_DEFINE(inst)							\
	DT_INST_FOREACH_PROP_ELEM_VARGS(inst, input, HID_GAMEPAD_INPUT_CB, inst)	\
											\
	static const struct hid_gamepad_config hid_gamepad_config_##inst = {		\
		.hid_dev = DEVICE_DT_GET(DT_INST_GPARENT(inst)),			\
		.input_id = DT_INST_PROP_BY_IDX(inst, input_id, 0),			\
	};										\
											\
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL, &hid_gamepad_config_##inst,	\
			      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(HID_GAMEPAD_DEFINE)
