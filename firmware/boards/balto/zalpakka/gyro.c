/* ported over from https://github.com/inputlabs/alpakka_firmware */
#include <math.h>
#include <stdlib.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT gyro

LOG_MODULE_REGISTER(gyro, LOG_LEVEL_INF);

#define IMU_OVERSAMPLING 64

static K_TIMER_DEFINE(gyro_sync, NULL, NULL);

static atomic_t gyro_active;

static const struct {
	const struct device *imu_l;
	const struct device *imu_r;
} cfg = {
	.imu_l = DEVICE_DT_GET(DT_INST_PHANDLE(0, imu_l)),
	.imu_r = DEVICE_DT_GET(DT_INST_PHANDLE(0, imu_r)),
};

struct vector {
	double x;
	double y;
	double z;
};

DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
		      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

static void imu_read_gyro_burst(const struct device *dev, struct vector *v,
				int samples)
{
	struct sensor_value x, y, z;

	for (int i = 0; i < samples; i++) {
		sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
		sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &x);
		sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &y);
		sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &z);

		v->x += sensor_rad_to_degrees(&x);
		v->y += sensor_rad_to_degrees(&y);
		v->z += sensor_rad_to_degrees(&z);
	}

	v->x /= samples;
	v->y /= samples;
	v->z /= samples;
}

#define ramp_mid(x, z)  ((x) < (z)) ? 0 : (x) > (1 - (z)) ? 1 : ((x) - (z)) * (1 / (1 - 2 * (z)))

static void imu_read_gyros(struct vector *v)
{
	struct vector v0, v1;
	double weight, weight_0, weight_1;

	memset(&v0, 0, sizeof(v0));
	imu_read_gyro_burst(cfg.imu_l, &v0, IMU_OVERSAMPLING / 8 * 1);
	/* scale back to raw values */
	v0.x = v0.x * INT16_MAX / 500;
	v0.y = v0.y * INT16_MAX / 500;
	v0.z = v0.z * INT16_MAX / 500;

	memset(&v1, 0, sizeof(v1));
	imu_read_gyro_burst(cfg.imu_r, &v1, IMU_OVERSAMPLING / 8 * 7);
	/* scale back to raw values */
	v1.x = v1.x * INT16_MAX / 125;
	v1.y = v1.y * INT16_MAX / 125;
	v1.z = v1.z * INT16_MAX / 125;

	weight = MAX(abs(v1.x), abs(v1.y)) / 32768.0;
	weight_0 = ramp_mid(weight, 0.2);
	weight_1 = 1 - weight_0;

	v->x = (v0.x * weight_0) + (v1.x * weight_1 / 4);
	v->y = (v0.y * weight_0) + (v1.y * weight_1 / 4);
	v->z = (v0.z * weight_0) + (v1.z * weight_1 / 4);
}

static double hssnf(double t, double k, double x)
{
	double a = x - (x * k);
	double b = 1 - (x * k * (1 / t));
	return a / b;
}

#define CFG_GYRO_SENSITIVITY (pow(2, -9) * 1.45) * 1.6
#define SENSITIVITY_MULTIPLIER 4.0 / 3.0
#define MAGIC_T 1.0
#define MAGIC_K 0.5

static void gyro_loop(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT));
	static double sub_x, sub_y, sub_z;
	int x, y, z;
	struct vector v;

	memset(&v, 0, sizeof(v));
	imu_read_gyros(&v);

	v.x *= CFG_GYRO_SENSITIVITY * SENSITIVITY_MULTIPLIER;
	v.y *= CFG_GYRO_SENSITIVITY * SENSITIVITY_MULTIPLIER;
	v.z *= CFG_GYRO_SENSITIVITY * SENSITIVITY_MULTIPLIER;

	if (v.x > 0 && v.x < MAGIC_T) {
		v.x =  hssnf(MAGIC_T, MAGIC_K,  v.x);
	} else if (v.x < 0 && v.x > -MAGIC_T) {
		v.x = -hssnf(MAGIC_T, MAGIC_K, -v.x);
	} if (v.y > 0 && v.y <  MAGIC_T) {
		v.y =  hssnf(MAGIC_T, MAGIC_K, v.y);
	} else if (v.y < 0 && v.y > -MAGIC_T) {
		v.y = -hssnf(MAGIC_T, MAGIC_K, -v.y);
	} if (v.z > 0 && v.z < MAGIC_T) {
		v.z =  hssnf(MAGIC_T, MAGIC_K,  v.z);
	} else if (v.z < 0 && v.z > -MAGIC_T) {
		v.z = -hssnf(MAGIC_T, MAGIC_K, -v.z);
	}

	// Reintroduce subpixel leftovers.
	v.x += sub_x;
	v.y += sub_y;
	v.z += sub_z;

	// Round down and save leftovers.
	sub_x = modf(v.x, &v.x);
	sub_y = modf(v.y, &v.y);
	sub_z = modf(v.z, &v.z);

	x = v.x;
	y = v.y;
	z = v.z;

	if (x || y || z) {
		input_report_rel(dev, INPUT_REL_X, -z, false, K_FOREVER);
		input_report_rel(dev, INPUT_REL_Y, x, true, K_FOREVER);
	}
}

static void imu_configure(const struct device *dev, int freq, int full_scale)
{
	struct sensor_value val;
	int ret;

	val.val1 = freq;
	val.val2 = 0;

	ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
			      SENSOR_ATTR_SAMPLING_FREQUENCY, &val);
	if (ret != 0) {
		LOG_ERR("%s: sampling frequency set failed: %d", dev->name, ret);
		return;
	}

	sensor_degrees_to_rad(full_scale, &val);

	ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
			      SENSOR_ATTR_FULL_SCALE, &val);
	if (ret != 0) {
		LOG_ERR("%s: scale set failed: %d", dev->name, ret);
	}
}

static void gyro_input_cb(struct input_event *evt)
{
	if (evt->code == INPUT_BTN_TOUCH) {
		if (evt->value) {
			atomic_set(&gyro_active, 1);
		} else {
			atomic_set(&gyro_active, 0);
		}
	}
}
INPUT_CALLBACK_DEFINE(NULL, gyro_input_cb);

static void gyro_thread(void)
{
	imu_configure(cfg.imu_l, 6660, 500);
	imu_configure(cfg.imu_r, 6660, 125);

	k_timer_start(&gyro_sync, K_MSEC(10), K_MSEC(10));

	while (true) {
		if (atomic_get(&gyro_active)) {
			gyro_loop();
		}
		k_timer_status_sync(&gyro_sync);
	}

	imu_configure(cfg.imu_l, 0, 500);
	imu_configure(cfg.imu_r, 0, 125);
}

K_THREAD_DEFINE(gyro, 1024, gyro_thread, NULL, NULL, NULL, 14, 0, 0);
