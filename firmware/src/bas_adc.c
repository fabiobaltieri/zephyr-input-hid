#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bas_adc, LOG_LEVEL_INF);

#define SOC_UPDATE_SECS 10

static const struct device *fuel_gauge = DEVICE_DT_GET_ONE(zephyr_fuel_gauge_composite);

static void bas_adc_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(bas_adc_work, bas_adc_handler);

static void bas_adc_handler(struct k_work *work)
{
	union fuel_gauge_prop_val val1;
	union fuel_gauge_prop_val val2;

	fuel_gauge_get_prop(fuel_gauge, FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE, &val1);
	fuel_gauge_get_prop(fuel_gauge, FUEL_GAUGE_VOLTAGE, &val2);

	LOG_INF("vbatt_mv=%d soc=%d", val2.voltage / 1000, val1.relative_state_of_charge);

	bt_bas_set_battery_level(val1.relative_state_of_charge);

	k_work_schedule(&bas_adc_work, K_SECONDS(SOC_UPDATE_SECS));
}

static int bas_soc_adc_init(void)
{
	if (!device_is_ready(fuel_gauge)) {
		LOG_ERR("fuel gauge device is not ready");
		return -ENODEV;
	}

	bas_adc_handler(&bas_adc_work.work);

	return 0;
}
SYS_INIT(bas_soc_adc_init, APPLICATION, 91);
