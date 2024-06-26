#include <zephyr/drivers/charger.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(charger, LOG_LEVEL_INF);

static const struct device *charger = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(charger));

static void charging_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(charging_dwork, charging_handler);

static void charging_handler(struct k_work *work)
{
	union charger_propval val0, val1;
	int ret;

	ret = charger_get_prop(charger, CHARGER_PROP_ONLINE, &val0);
	if (ret < 0) {
		goto out;
	}

	ret = charger_get_prop(charger, CHARGER_PROP_STATUS, &val1);
	if (ret < 0) {
		goto out;
	}

	if (val0.online) {
		LOG_INF("charger status: %d", val1.status);
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
