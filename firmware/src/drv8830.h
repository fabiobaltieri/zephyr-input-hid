#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

enum drv8830_mode {
	DRV8830_MODE_STANDBY = 0x00,
	DRV8830_MODE_REVERSE = 0x01,
	DRV8830_MODE_FORWARD = 0x02,
	DRV8830_MODE_BREAK = 0x03,
};

int drv8830_config_set(const struct device *dev,
		       uint8_t vset, enum drv8830_mode mode);
