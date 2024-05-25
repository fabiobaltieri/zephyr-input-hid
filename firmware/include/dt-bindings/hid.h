#define HID_KBD_KEYS_CODES_SIZE 6
#define HID_KBD_KEYS_BITS_SIZE 16

#define HID_KBD_REPORT_HEADER(id)						\
	0x05 0x01			/* Usage Page (Generic Desktop) */	\
	0x09 0x06			/* Usage (Keyboard) */			\
	0xa1 0x01			/* Collection (Application) */		\
	0x85 id				/*  Report ID (id) */			\
	0x05 0x07			/*  Usage Page (Keyboard) */		\
	0x19 0xe0			/*  Usage Minimum (224) */		\
	0x29 0xe7			/*  Usage Maximum (231) */		\
	0x15 0x00			/*  Logical Minimum (0) */		\
	0x25 0x01			/*  Logical Maximum (1) */		\
	0x95 0x08			/*  Report Count (8) */			\
	0x75 0x01			/*  Report Size (1) */			\
	0x81 0x02			/*  Input (Data,Var,Abs) */		\
	0x95 0x01			/*  Report Count (1) */			\
	0x75 0x08			/*  Report Size (8) */			\
	0x81 0x01			/*  Input (Cnst,Arr,Abs) */		\
	0x05 0x07			/*  Usage Page (Keyboard) */

#define HID_KBD_REPORT_TRAILER							\
	0xc0				/* End Collection */

#define HID_KBD_REPORT_LED							\
	0x05 0x08			/*  Usage Page (LEDs) */		\
	0x19 0x01			/*  Usage Minimum (1) */		\
	0x29 0x05			/*  Usage Maximum (5) */		\
	0x95 0x05			/*  Report Count (5) */			\
	0x75 0x01			/*  Report Size (1) */			\
	0x91 0x02			/*  Output (Data,Var,Abs) */		\
	0x95 0x01			/*  Report Count (1) */			\
	0x75 0x03			/*  Report Size (3) */			\
	0x91 0x01			/*  Output (Cnst,Arr,Abs) */		\

#define HID_KBD_REPORT(id, led)							\
	HID_KBD_REPORT_HEADER(id)						\
	0x19 0x00			/*  Usage Minimum (0) */		\
	0x29 0xff			/*  Usage Maximum (255) */		\
	0x15 0x00			/*  Logical Minimum (0) */		\
	0x25 0xff			/*  Logical Maximum (255) */		\
	0x95 HID_KBD_KEYS_CODES_SIZE	/*  Report Count () */			\
	0x75 0x08			/*  Report Size (8) */			\
	0x81 0x00			/*  Input (Data,Arr,Abs) */		\
	IF_ENABLED(led, (HID_KBD_REPORT_LED))					\
	HID_KBD_REPORT_TRAILER

#define HID_KBD_REPORT_NKRO(id, led)						\
	HID_KBD_REPORT_HEADER(id)						\
	0x19 0x00			/*  Usage Minimum (0) */		\
	0x29 0x7f			/*  Usage Maximum (127) */		\
	0x15 0x00			/*  Logical Minimum (0) */		\
	0x25 0x01			/*  Logical Maximum (1) */		\
	0x95 0x80			/*  Report Count (128) */		\
	0x75 0x01			/*  Report Size (1) */			\
	0x81 0x02			/*  Input (Data,Var,Abs) */		\
	IF_ENABLED(led, (HID_KBD_REPORT_LED))					\
	HID_KBD_REPORT_TRAILER

#define HID_MOUSE_REPORT(id)					\
	0x05 0x01	/* Usage Page (Generic Desktop) */	\
	0x09 0x02	/* Usage (Mouse) */			\
	0xa1 0x01	/* Collection (Application) */		\
	0x85 id		/*  Report ID (id) */			\
	0x09 0x01	/*  Usage (Pointer) */			\
	0xa1 0x00	/*  Collection (Physical) */		\
								\
	0x05 0x09	/*   Usage Page (Button) */		\
	0x19 0x01	/*   Usage Minimum (1) */		\
	0x29 0x08	/*   Usage Maximum (8) */		\
	0x15 0x00	/*   Logical Minimum (0) */		\
	0x25 0x01	/*   Logical Maximum (1) */		\
	0x95 0x08	/*   Report Count (8) */		\
	0x75 0x01	/*   Report Size (1) */			\
	0x81 0x02	/*   Input (Data,Var,Abs) */		\
								\
	0x05 0x01	/*   Usage Page (Generic Desktop) */    \
	0x16 0x01 0x80	/*   Logical Minimum (-32767) */	\
	0x26 0xff 0x7f	/*   Logical Maximum (32767) */		\
	0x75 0x10	/*   Report Size (16) */		\
	0x95 0x02	/*   Report Count (2) */		\
	0x09 0x30	/*   Usage (X) */			\
	0x09 0x31	/*   Usage (Y) */			\
	0x81 0x06	/*   Input (Data,Var,Rel) */		\
								\
	0x15 0x81	/*   Logical Minimum (-127) */		\
	0x25 0x7f	/*   Logical Maximum (127) */		\
	0x75 0x08	/*   Report Size (8) */			\
	0x95 0x01	/*   Report Count (1) */		\
	0x09 0x38	/*   Usage (Wheel) */			\
	0x81 0x06	/*   Input (Data,Var,Rel) */		\
								\
	0xc0		/*  End Collection */			\
	0xc0		/* End Collection */

#define HID_GAMEPAD_REPORT(id)							\
	0x05 0x01		/* Usage Page (Generic Desktop) */		\
	0x09 0x05		/* Usage (Game Pad) */				\
	0xa1 0x01		/* Collection (Application) */			\
	0x85 id			/*  Report ID (id) */				\
										\
	0x05 0x01		/*  Usage Page (Generic Desktop) */		\
	0x75 0x04		/*  Report Size (4) */				\
	0x95 0x01		/*  Report Count (1) */				\
	0x25 0x07		/*  Logical Maximum (7) */			\
	0x46 0x3b 0x01		/*  Physical Maximum (315) */			\
	0x65 0x14		/*  Unit (Degrees,EngRotation) */		\
	0x09 0x39		/*  Usage (Hat switch) */			\
	0x81 0x42		/*  Input (Data,Var,Abs,Null) */		\
	0x45 0x00		/*  Physical Maximum (0) */			\
	0x65 0x00		/*  Unit (None) */				\
										\
	0x75 0x01		/*  Report Size (1) */				\
	0x95 0x04		/*  Report Count (4) */				\
	0x81 0x01		/*  Input (Cnst,Arr,Abs) */			\
										\
	0x05 0x09		/*  Usage Page (Button) */			\
	0x15 0x00		/*  Logical Minimum (0) */			\
	0x25 0x01		/*  Logical Maximum (1) */			\
	0x75 0x01		/*  Report Size (1) */				\
	0x95 0x0d		/*  Report Count (13) */			\
	0x09 0x01		/*  Usage (BTN_SOUTH) */			\
	0x09 0x02		/*  Usage (BTN_EAST) */				\
	0x09 0x04		/*  Usage (BTN_NORTH) */			\
	0x09 0x05		/*  Usage (BTN_WEST) */				\
	0x09 0x07		/*  Usage (BTN_TL) */				\
	0x09 0x08		/*  Usage (BTN_TR) */				\
	0x09 0x0b		/*  Usage (BTN_SELECT) */			\
	0x09 0x0c		/*  Usage (BTN_START) */			\
	0x09 0x0d		/*  Usage (BTN_MODE) */				\
	0x09 0x0e		/*  Usage (BTN_THUMBL) */			\
	0x09 0x0f		/*  Usage (BTN_THUMBR) */			\
	0x09 0x11		/*  Usage (BTN_TRIGGER_HAPPY1) */		\
	0x09 0x12		/*  Usage (BTN_TRIGGER_HAPPY2) */		\
	0x81 0x02		/*  Input (Data,Var,Abs) */			\
										\
	0x75 0x01		/*  Report Size (1) */				\
	0x95 0x03		/*  Report Count (3) */				\
	0x81 0x01		/*  Input (Cnst,Arr,Abs) */			\
										\
	0x05 0x01		/*  Usage Page (Generic Desktop) */		\
	0x15 0x01		/*  Logical Minimum (1) */			\
	0x26 0xff 0x00		/*  Logical Maximum (255) */			\
	0x09 0x01		/*  Usage (Pointer) */				\
	0xa1 0x00		/*  Collection (Physical) */			\
	0x09 0x30		/*   Usage (X) */				\
	0x09 0x31		/*   Usage (Y) */				\
	0x75 0x08		/*   Report Size (8) */				\
	0x95 0x02		/*   Report Count (2) */			\
	0x81 0x02		/*   Input (Data,Var,Abs) */			\
	0xc0			/*  End Collection */				\
										\
	0x09 0x01		/*  Usage (Pointer) */				\
	0xa1 0x00		/*  Collection (Physical) */			\
	0x09 0x33		/*   Usage (Rx) */				\
	0x09 0x34		/*   Usage (Ry) */				\
	0x75 0x08		/*   Report Size (8) */				\
	0x95 0x02		/*   Report Count (2) */			\
	0x81 0x02		/*   Input (Data,Var,Abs) */			\
	0xc0			/*  End Collection */				\
										\
	0x85 0x02		/*  Report ID (2) */				\
	0x06 0x0f 0x00		/*  Usage Page (Vendor Usage Page 0x0f) */	\
	0x09 0x97		/*  Usage (Vendor Usage 0x97) */		\
	0x75 0x08		/*  Report Size (8) */				\
	0x95 0x02		/*  Report Count (2) */				\
	0x26 0xff 0x00		/*  Logical Maximum (255) */			\
	0x91 0x02		/*  Output (Data,Var,Abs) */			\
										\
	0xc0			/* End Collection */
