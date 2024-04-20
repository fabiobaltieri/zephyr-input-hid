#include <stdint.h>

#include "hid_kbd.h"
#include "hid_report_map.h"

const uint8_t hid_report_map[] = {
#if CONFIG_KBD_HID_NKRO
	HID_KBD_REPORT_NKRO(HID_REPORT_ID_KBD),
#else
	HID_KBD_REPORT(HID_REPORT_ID_KBD),
#endif
};

const uint16_t hid_report_map_len = sizeof(hid_report_map);
