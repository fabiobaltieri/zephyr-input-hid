#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "drv8830.h"

LOG_MODULE_REGISTER(DRV8830, CONFIG_SENSOR_LOG_LEVEL);

#define DT_DRV_COMPAT ti_drv8830

/* Registers */
#define DRV8830_CONTROL 0x00
#define DRV8830_FAULT   0x01

/* Fault IDs */
#define DRV8830_FAULT_FAULT  0x01
#define DRV8830_FAULT_OCP    0x02
#define DRV8830_FAULT_UVLO   0x04
#define DRV8830_FAULT_UTS    0x08
#define DRV8830_FAULT_ILIMIT 0x10
#define DRV8830_FAULT_CLEAR  0x80

#define DRV8830_VSET_MAX 0x3f

#define DRV8830_VREF 1.285f

struct drv8830_config {
	struct i2c_dt_spec i2c;
};

struct drv8830_data {
	uint8_t vset;
};

static int drv8830_reg_read(const struct device *dev,
			    uint8_t reg_addr, uint8_t *reg_value)
{
	const struct drv8830_config *cfg = dev->config;

	return i2c_reg_read_byte_dt(&cfg->i2c, reg_addr, reg_value);
}

static int drv8830_reg_write(const struct device *dev,
			     uint8_t reg_addr, uint8_t reg_value)
{
	const struct drv8830_config *cfg = dev->config;

	return i2c_reg_write_byte_dt(&cfg->i2c, reg_addr, reg_value);
}

int drv8830_config_set(const struct device *dev,
		       uint8_t vset, enum drv8830_mode mode)
{
	uint8_t val;
	int rc;

	if (vset > DRV8830_VSET_MAX) {
		return -EINVAL;
	}

	val = (vset << 2) | mode;

	rc = drv8830_reg_write(dev, DRV8830_CONTROL, val);
	if (rc) {
		LOG_ERR("drv8830_reg_write failed: %d", rc);
		return rc;
	}

	return 0;
}

static int drv8830_sample_fetch(const struct device *dev,
				enum sensor_channel chan)
{
	struct drv8830_data *data = dev->data;
	uint8_t val;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	drv8830_reg_read(dev, DRV8830_CONTROL, &val);

	data->vset = val >> 2;

	return 0;
}

static int drv8830_channel_get(const struct device *dev,
			       enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct drv8830_data *data = dev->data;
	double tmp;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	tmp = 4 * DRV8830_VREF * data->vset / 64;

	return sensor_value_from_double(val, tmp);
}

static int drv8830_init(const struct device *dev)
{
	const struct drv8830_config *cfg = dev->config;
	int rc;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus device not ready.");
		return -ENODEV;
	}

	rc = drv8830_reg_write(dev, DRV8830_CONTROL, DRV8830_MODE_STANDBY);
	if (rc) {
		LOG_ERR("Could not initialize the control register: %d", rc);
		return rc;
	}

	rc = drv8830_reg_write(dev, DRV8830_FAULT, DRV8830_FAULT_CLEAR);
	if (rc) {
		LOG_ERR("Could not clear the fault register: %d", rc);
		return rc;
	}

	return 0;
}

static const struct sensor_driver_api drv8830_api = {
	.sample_fetch = drv8830_sample_fetch,
	.channel_get = drv8830_channel_get,
};

#define DRV8830_INIT(n) \
	static struct drv8830_data drv8830_data_##n; \
		\
	static const struct drv8830_config drv8830_config_##n = { \
		.i2c = I2C_DT_SPEC_INST_GET(n), \
	}; \
		\
	SENSOR_DEVICE_DT_INST_DEFINE(n, drv8830_init, NULL, \
				     &drv8830_data_##n, &drv8830_config_##n, \
				     POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, \
				     &drv8830_api);

DT_INST_FOREACH_STATUS_OKAY(DRV8830_INIT)
