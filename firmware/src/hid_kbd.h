#include <stdint.h>
#include <zephyr/input/input.h>
#include <zephyr/toolchain.h>

#define HID_KBD_REPORT(id)						\
	0x05, 0x01,		/* Usage Page (Generic Desktop) */	\
	0x09, 0x06,		/* Usage (Keyboard) */			\
	0xa1, 0x01,		/* Collection (Application) */		\
	0x85, id,		/*  Report ID (id) */			\
	0x05, 0x07,		/*  Usage Page (Keyboard) */		\
	0x19, 0xe0,		/*  Usage Minimum (224) */		\
	0x29, 0xe7,		/*  Usage Maximum (231) */		\
	0x15, 0x00,		/*  Logical Minimum (0) */		\
	0x25, 0x01,		/*  Logical Maximum (1) */		\
	0x95, 0x08,		/*  Report Count (8) */			\
	0x75, 0x01,		/*  Report Size (1) */			\
	0x81, 0x02,		/*  Input (Data,Var,Abs) */		\
	0x95, 0x01,		/*  Report Count (1) */			\
	0x75, 0x08,		/*  Report Size (8) */			\
	0x81, 0x01,		/*  Input (Cnst,Arr,Abs) */		\
	0x05, 0x07,		/*  Usage Page (Keyboard) */		\
	0x19, 0x00,		/*  Usage Minimum (0) */		\
	0x2a, 0xff, 0x00,	/*  Usage Maximum (255) */		\
	0x15, 0x00,		/*  Logical Minimum (0) */		\
	0x26, 0xff, 0x00,	/*  Logical Maximum (255) */		\
	0x95, 0x06,		/*  Report Count (6) */			\
	0x75, 0x08,		/*  Report Size (8) */			\
	0x81, 0x00,		/*  Input (Data,Arr,Abs) */		\
	0xc0			/* End Collection */

#define HID_KBD_KEYS_REPORT_SIZE 6

struct hid_kbd_report {
	uint8_t modifiers;
	uint8_t _reserved;
	uint8_t keys[HID_KBD_KEYS_REPORT_SIZE];
} __packed;

struct hid_kbd_report_id {
	uint8_t id;
	struct hid_kbd_report report;
} __packed;

void hid_kbd_input_process(struct hid_kbd_report *report,
			   struct input_event *evt);
