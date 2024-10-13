#define DT_DRV_COMPAT pm_control

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>

#include "event.h"

LOG_MODULE_REGISTER(pm_control, LOG_LEVEL_INF);

#define DEVICE_DT_GET_BY_IDX(node, prop, idx) \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node, prop, idx))

static const struct device *always_on[] = {
#if DT_INST_NODE_HAS_PROP(0, always_on)
	DT_INST_FOREACH_PROP_ELEM_SEP(0, always_on, DEVICE_DT_GET_BY_IDX, (,))
#endif
};

static const struct device *connect_on[] = {
	DT_INST_FOREACH_PROP_ELEM_SEP(0, connect_on, DEVICE_DT_GET_BY_IDX, (,))
};

static void pm_control_cb(enum event_code code)
{
	uint8_t i;

	switch (code) {
	case EVENT_BOOT:
		for (i = 0; i < ARRAY_SIZE(always_on); i++) {
			pm_device_runtime_get(always_on[i]);
		}
		break;
	case EVENT_BT_CONNECTED:
	case EVENT_USB_CONNECTED:
		for (i = 0; i < ARRAY_SIZE(connect_on); i++) {
			pm_device_runtime_get(connect_on[i]);
		}
		break;
	case EVENT_BT_DISCONNECTED:
	case EVENT_USB_DISCONNECTED:
		for (i = 0; i < ARRAY_SIZE(connect_on); i++) {
			pm_device_runtime_put(connect_on[i]);
		}
		break;
	default:
	}
}
EVENT_CALLBACK_DEFINE(pm_control_cb);
