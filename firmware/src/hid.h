#include <stdint.h>

struct hid_config {
	const uint8_t *report_map;
	uint16_t report_map_len;
};

static inline const uint8_t *hid_dev_report(const struct device *dev)
{
	const struct hid_config *cfg = dev->config;

	return cfg->report_map;
}

static inline uint16_t hid_dev_report_len(const struct device *dev)
{
	const struct hid_config *cfg = dev->config;

	return cfg->report_map_len;
}

#define HID_REPORT_ID_KBD 1
