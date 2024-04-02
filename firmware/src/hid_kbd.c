#include <stdint.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_hid.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#include "hid_kbd.h"

LOG_MODULE_REGISTER(hid_kbd, LOG_LEVEL_INF);

static void hid_kbd_set_key_keyboard(struct hid_kbd_report *report,
				     uint16_t code, uint32_t value)
{
	int hid_code;
	uint8_t modifier;

	modifier = input_to_hid_modifier(code);
	if (modifier != 0) {
		if (value) {
			report->modifiers |= modifier;
		} else {
			report->modifiers &= ~modifier;
		}
	}

	/* normal keys */
	hid_code = input_to_hid_code(code);
#ifdef HID_KBD_NKRO
	uint8_t off;
	uint8_t bit;

	off = hid_code / 8;
	bit = hid_code % 8;

	if (off >= HID_KBD_KEYS_REPORT_SIZE) {
		LOG_ERR("invalid code: %d", hid_code);
		return;
	}

	WRITE_BIT(report->keys[off], bit, value);
#else
	int i;
	if (hid_code < 0) {
		return;
	}

	if (value) {
		for (i = 0; i < HID_KBD_KEYS_REPORT_SIZE; i++) {
			if (report->keys[i] == 0x00) {
				report->keys[i] = hid_code;
				return;
			}
		}
	} else {
		for (i = 0; i < HID_KBD_KEYS_REPORT_SIZE; i++) {
			if (report->keys[i] == hid_code) {
				report->keys[i] = 0x00;
				return;
			}
		}
	}
#endif
}

void hid_kbd_input_process(struct hid_kbd_report *report,
			   struct input_event *evt) {
	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	hid_kbd_set_key_keyboard(report, evt->code, evt->value);
}
