#include <stdint.h>
#include <zephyr/device.h>

const uint8_t *hid_dev_report(const struct device *dev);

uint16_t hid_dev_report_len(const struct device *dev);
