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

#define HID_KBD_REPORT(id)							\
	HID_KBD_REPORT_HEADER(id)						\
	0x19 0x00			/*  Usage Minimum (0) */		\
	0x29 0xff			/*  Usage Maximum (255) */		\
	0x15 0x00			/*  Logical Minimum (0) */		\
	0x25 0xff			/*  Logical Maximum (255) */		\
	0x95 HID_KBD_KEYS_CODES_SIZE	/*  Report Count () */			\
	0x75 0x08			/*  Report Size (8) */			\
	0x81 0x00			/*  Input (Data,Arr,Abs) */		\
	HID_KBD_REPORT_TRAILER

#define HID_KBD_REPORT_NKRO(id)							\
	HID_KBD_REPORT_HEADER(id)						\
	0x19 0x00			/*  Usage Minimum (0) */		\
	0x29 0x7f			/*  Usage Maximum (127) */		\
	0x15 0x00			/*  Logical Minimum (0) */		\
	0x25 0x01			/*  Logical Maximum (1) */		\
	0x95 0x80			/*  Report Count (128) */		\
	0x75 0x01			/*  Report Size (1) */			\
	0x81 0x02			/*  Input (Data,Var,Abs) */		\
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
