#include <stdint.h>
#include <zephyr/input/input.h>
#include <zephyr/toolchain.h>
#include <dt-bindings/hid.h>

struct hid_kbd_report {
	uint8_t modifiers;
	uint8_t _reserved;
	uint8_t codes[HID_KBD_KEYS_CODES_SIZE];
} __packed;

struct hid_kbd_report_id {
	uint8_t id;
	struct hid_kbd_report report;
} __packed;

struct hid_kbd_report_data {
	uint8_t bits[HID_KBD_KEYS_BITS_SIZE];
};

void hid_kbd_input_process(struct hid_kbd_report *report,
			   struct hid_kbd_report_data *data,
			   struct input_event *evt);

struct hid_kbd_report_nkro {
	uint8_t modifiers;
	uint8_t _reserved;
	uint8_t bits[HID_KBD_KEYS_BITS_SIZE];
} __packed;

struct hid_kbd_report_nkro_id {
	uint8_t id;
	struct hid_kbd_report_nkro report;
} __packed;

void hid_kbd_input_process_nkro(struct hid_kbd_report_nkro *report,
				struct input_event *evt);
