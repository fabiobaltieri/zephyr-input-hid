#include <zephyr/device.h>
#include <stdint.h>

typedef int (*hid_mouse_feat_cb)(const struct device *dev,
				 uint8_t report_id, const uint8_t *buf, uint8_t len);

void hid_mouse_register_feat_cb(const struct device *dev, hid_mouse_feat_cb cb);
