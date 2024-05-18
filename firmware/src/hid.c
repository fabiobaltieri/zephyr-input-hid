#define DT_DRV_COMPAT hid

#include <stdint.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>

#include "hid.h"

#define HID_REPORT_BYTE(node_id, prop, idx) \
	DT_PROP_BY_IDX(node_id, prop, idx),

#define HID_REPORT_DEVICE(node_id) \
	DT_FOREACH_PROP_ELEM(node_id, report, HID_REPORT_BYTE)

#define HID_INIT(n)								\
	const uint8_t hid_report_map_##n[] = {					\
		DT_INST_FOREACH_CHILD(n, HID_REPORT_DEVICE)			\
	};									\
										\
	static const struct hid_config hid_cfg_##n = {				\
		.report_map = hid_report_map_##n,				\
		.report_map_len = sizeof(hid_report_map_##n),			\
	};									\
										\
	DEVICE_DT_INST_DEFINE(n, NULL, NULL,					\
			      NULL, &hid_cfg_##n,				\
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,	\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(HID_INIT)
