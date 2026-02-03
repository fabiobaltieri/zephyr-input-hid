#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(reboot, LOG_LEVEL_INF);

#if DT_NODE_HAS_STATUS_OKAY(DT_N_NODELABEL_reboot)

static void reboot_cb(struct input_event *evt, void *user_data)
{
	if (!(evt->type == INPUT_EV_KEY &&
	      evt->code == INPUT_KEY_SYSRQ)) {
		return;
	}

	LOG_ERR("reboot key");

	sys_reboot(SYS_REBOOT_COLD);
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(reboot)), reboot_cb, NULL);

#endif
