#include <stdint.h>
#include <zephyr/devicetree.h>

#include "hid.h"

#define DT_DRV_COMPAT hid

#define HID_REPORT_BYTE(node_id, prop, idx) \
	DT_PROP_BY_IDX(node_id, prop, idx),

#define HID_REPORT_DEVICE(node_id) \
	DT_FOREACH_PROP_ELEM(node_id, report, HID_REPORT_BYTE)

const uint8_t hid_report_map[] = {
	DT_INST_FOREACH_CHILD(0, HID_REPORT_DEVICE)
};

const uint16_t hid_report_map_len = sizeof(hid_report_map);
