#include <zephyr/drivers/charger.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "event.h"

LOG_MODULE_REGISTER(charger, LOG_LEVEL_INF);

static const struct device *charger = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(charger));

static void charging_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(charging_dwork, charging_handler);

static void charging_handler(struct k_work *work)
{
	union charger_propval val0, val1;
	static enum event_code last_code = EVENT_CHARGER_DISCONNECTED;
	static enum event_code code;
	int ret;

	ret = charger_get_prop(charger, CHARGER_PROP_ONLINE, &val0);
	if (ret < 0) {
		goto out;
	}

	ret = charger_get_prop(charger, CHARGER_PROP_STATUS, &val1);
	if (ret < 0) {
		goto out;
	}

	if (val0.online == CHARGER_ONLINE_OFFLINE) {
		code = EVENT_CHARGER_DISCONNECTED;
	} else if (val1.status == CHARGER_STATUS_CHARGING) {
		code = EVENT_CHARGER_CHARGING;
	} else if (val1.status == CHARGER_STATUS_FULL) {
		code = EVENT_CHARGER_FULL;
	} else {
		LOG_WRN("unknown charger state: oneline=%d status=%d",
			val0.online, val1.status);
		goto out;
	}

	if (code != last_code) {
		event(code);
		last_code = code;
	}

out:
        k_work_schedule(&charging_dwork, K_MSEC(1000));
}

static int charging_start(void)
{
	if (charger == NULL) {
		LOG_INF("no charger defined");
		return 0;
	}

	charging_handler(&charging_dwork.work);

	return 0;
}
SYS_INIT(charging_start, APPLICATION, 99);
