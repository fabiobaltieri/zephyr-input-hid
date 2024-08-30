#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bas_fuel_gauge, LOG_LEVEL_INF);

#define SOC_UPDATE_SECS 10

static const struct device *fuel_gauge = DEVICE_DT_GET(DT_NODELABEL(fuel_gauge_sensor));

static void bas_fuel_gauge_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(fuel_gauge_dwork, bas_fuel_gauge_handler);

static void bas_fuel_gauge_handler(struct k_work *work)
{
	struct sensor_value val;

	sensor_sample_fetch_chan(fuel_gauge, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE);
	sensor_channel_get(fuel_gauge, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &val);

	LOG_DBG("soc: %d", val.val1);

	bt_bas_set_battery_level(val.val1);

	k_work_schedule(&fuel_gauge_dwork, K_SECONDS(SOC_UPDATE_SECS));
}

static int bas_fuel_gauge_init(void)
{
	if (!device_is_ready(fuel_gauge)) {
		LOG_ERR("fuel gauge device is not ready");
		return -ENODEV;
	}

	bas_fuel_gauge_handler(&fuel_gauge_dwork.work);

	return 0;
}
SYS_INIT(bas_fuel_gauge_init, APPLICATION, 91);
