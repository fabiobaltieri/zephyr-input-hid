#include <stdint.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_hid.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#include "hid_kbd.h"

LOG_MODULE_REGISTER(hid_kbd, LOG_LEVEL_INF);

#define HID_ROLLOVER_CODE 0x01

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

void hid_kbd_input_process(struct hid_kbd_report *report,
			   struct hid_kbd_report_data *data,
			   struct input_event *evt)
{
	int ret;
	uint8_t bits_count;
	uint8_t code_off;
	uint8_t i;

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	hid_kbd_update_modifiers(&report->modifiers, evt);

	ret = hid_kbd_update_bits(data->bits, evt);
	if (ret < 0) {
		return;
	}

	bits_count = 0;
	for (i = 0; i < ARRAY_SIZE(data->bits); i++) {
		bits_count += POPCOUNT(data->bits[i]);
	}

	if (bits_count > HID_KBD_KEYS_CODES_SIZE) {
		memset(report->codes, HID_ROLLOVER_CODE, sizeof(report->codes));
		return;
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
}

void hid_kbd_input_process_nkro(struct hid_kbd_report_nkro *report,
				struct input_event *evt)
{
	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	hid_kbd_update_modifiers(&report->modifiers, evt);

	hid_kbd_update_bits(report->bits, evt);
}
