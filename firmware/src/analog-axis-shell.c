#include <zephyr/device.h>
#include <zephyr/input/input_analog_axis.h>
#include <zephyr/input/input_analog_axis_settings.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

static void raw_print_cb(const struct device *dev, int channel, int16_t raw_val)
{
	printk("%s: %d: %d\n", dev->name, channel, raw_val);
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
	printk("%d %d %d\n", cal.in_min, cal.in_max, cal.in_deadzone);

	err = analog_axis_calibration_set(dev, chan, &cal);

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
       SHELL_CMD_ARG(raw_print, &dsub_device_name, "raw_print",
                     cmd_analog_axis_raw_print, 3, 0),
       SHELL_CMD_ARG(cal_set, &dsub_device_name, "cal_set CHAN IN_MIN IN_MAX IN_DEADZONE",
                     cmd_analog_axis_calibration_set, 6, 0),
       SHELL_CMD_ARG(cal_save, &dsub_device_name, "cal_save",
                     cmd_analog_axis_calibration_save, 2, 0),
       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(analog_axis, &sub_analog_axis_cmds, "Analog axis commands", NULL);
