#include <zephyr/device.h>
#include <zephyr/input/input_analog_axis.h>
#include <zephyr/input/input_analog_axis_settings.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(aa_shell, LOG_LEVEL_INF);

#define MAX_AXIS 6
#define DEADZONE_PCT 5

static struct {
	int16_t min;
	int16_t max;
	int16_t last;
	bool valid;
} cal_data[MAX_AXIS];

static void cal_cb(const struct device *dev, int channel, int16_t raw_val)
{
	static uint32_t last_ts;

	if (channel >= MAX_AXIS) {
		LOG_WRN("invalid channel %d", channel);
		return;
	}

	if (raw_val > cal_data[channel].max) {
		cal_data[channel].max = raw_val;
	}

	if (raw_val < cal_data[channel].min) {
		cal_data[channel].min = raw_val;
	}

	cal_data[channel].last = raw_val;
	cal_data[channel].valid = true;

	if (k_uptime_get_32() - last_ts < 500) {
		return;
	}

	LOG_INF("%s: last data", dev->name);
	for (uint8_t i = 0; i < MAX_AXIS; i++) {
		if (!cal_data[i].valid) {
			continue;
		}

		LOG_INF("chan=%d min=%d max=%d last=%d valid=%d",
			i,
			cal_data[i].min,
			cal_data[i].max,
			cal_data[i].last,
			cal_data[i].valid);
	}

	last_ts = k_uptime_get_32();
}

static void update_cal(const struct device *dev, int channel)
{
	struct analog_axis_calibration cal;
	int16_t range;
	int16_t deadzone;
	int ret;

	if (!cal_data[channel].valid) {
		return;
	}

	ret = analog_axis_calibration_get(dev, channel, &cal);
	if (ret < 0) {
		LOG_WRN("calibration get error: %d", ret);
		return;
	}

	range = cal_data[channel].max - cal_data[channel].min;
	deadzone = range * DEADZONE_PCT / 100;

	if (cal.in_deadzone != 0) {
		cal.in_deadzone = deadzone;
	}
	cal.in_min = cal_data[channel].min + deadzone;
	cal.in_max = cal_data[channel].max - deadzone;

	LOG_INF("new settings: chan=%d min=%d max=%d deadzone=%d",
		channel,
		cal.in_min, cal.in_max, cal.in_deadzone);

	ret = analog_axis_calibration_set(dev, channel, &cal);
	if (ret < 0) {
		LOG_ERR("calibration set error: %d", ret);
		return;
	}
}

static int cmd_analog_axis_cal(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	bool start;
	int err = 0;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Device %s not available", argv[1]);
		return -ENODEV;
	}

	start = shell_strtobool(argv[2], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[2]);
		return err;
	}

	if (start) {
		for (uint8_t i = 0; i < MAX_AXIS; i++) {
			cal_data[i].min = INT16_MAX;
			cal_data[i].max = INT16_MIN;
			cal_data[i].valid = false;
		}

		analog_axis_set_raw_data_cb(dev, cal_cb);
	} else {
		analog_axis_set_raw_data_cb(dev, NULL);

		for (uint8_t i = 0; i < MAX_AXIS; i++) {
			update_cal(dev, i);
		}
	}

	return 0;
}

static void raw_print_cb(const struct device *dev, int channel, int16_t raw_val)
{
	LOG_INF("%s: chan=%d val=%d", dev->name, channel, raw_val);
}

static int cmd_analog_axis_raw_print(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	bool enable;
	int err = 0;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Device %s not available", argv[1]);
		return -ENODEV;
	}

	enable = shell_strtobool(argv[2], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[2]);
		return err;
	}

	if (enable) {
		analog_axis_set_raw_data_cb(dev, raw_print_cb);
	} else {
		analog_axis_set_raw_data_cb(dev, NULL);
	}

	return 0;
}

static int cmd_analog_axis_calibration_set(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	struct analog_axis_calibration cal;
	int chan;
	int err = 0;
	int16_t in_min;
	int16_t in_max;
	int16_t in_deadzone;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Device %s not available", argv[1]);
		return -ENODEV;
	}

	chan = shell_strtoul(argv[2], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[2]);
		return err;
	}

	in_min = shell_strtoul(argv[3], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[3]);
		return err;
	}

	in_max = shell_strtoul(argv[4], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[4]);
		return err;
	}

	in_deadzone = shell_strtoul(argv[5], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[4]);
		return err;
	}

	err = analog_axis_calibration_get(dev, chan, &cal);

	cal.in_min = in_min;
	cal.in_max = in_max;
	cal.in_deadzone = in_deadzone;
	LOG_INF("min=%d max=%d deadzone=%d", cal.in_min, cal.in_max, cal.in_deadzone);

	err = analog_axis_calibration_set(dev, chan, &cal);
	if (err < 0) {
		shell_error(sh, "calibration set error: %d", err);
		return err;
	}

	return 0;
}

static int cmd_analog_axis_calibration_get(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	struct analog_axis_calibration cal;
	int ret;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Device %s not available", argv[1]);
		return -ENODEV;
	}

	for (uint8_t i = 0; i < 16; i++) {
		ret = analog_axis_calibration_get(dev, i, &cal);
		if (ret < 0) {
			return 0;
		}

		LOG_INF("%s: chan=%d min=%d max=%d deadzone=%d",
			dev->name,
			i,
			cal.in_min, cal.in_max, cal.in_deadzone);
	}

	return 0;
}

static int cmd_analog_axis_calibration_save(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Device %s not available", argv[1]);
		return -ENODEV;
	}

	analog_axis_calibration_save(dev);

	return 0;
}

static void device_name_get(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_lookup(idx, NULL);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(dsub_device_name, device_name_get);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_analog_axis_cmds,
       SHELL_CMD_ARG(cal, &dsub_device_name, "cal on|off",
                     cmd_analog_axis_cal, 3, 0),
       SHELL_CMD_ARG(raw_print, &dsub_device_name, "raw_print on|off",
                     cmd_analog_axis_raw_print, 3, 0),
       SHELL_CMD_ARG(cal_set, &dsub_device_name, "cal_set CHAN IN_MIN IN_MAX IN_DEADZONE",
                     cmd_analog_axis_calibration_set, 6, 0),
       SHELL_CMD_ARG(cal_get, &dsub_device_name, "cal_get",
                     cmd_analog_axis_calibration_get, 2, 0),
       SHELL_CMD_ARG(cal_save, &dsub_device_name, "cal_save",
                     cmd_analog_axis_calibration_save, 2, 0),
       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(analog_axis, &sub_analog_axis_cmds, "Analog axis commands", NULL);
