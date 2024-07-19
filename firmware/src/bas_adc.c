#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(bas_adc, LOG_LEVEL_INF);

#define SOC_UPDATE_SECS 10

#define VBATT_NODE DT_NODELABEL(vbatt)
#define VBATT_FULL_MV (4200 - 50)
#define VBATT_EMPTY_MV (3000 + 50)
static const struct adc_dt_spec vbatt = ADC_DT_SPEC_GET(VBATT_NODE);

static void soc_adc_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(soc_adc_dwork, soc_adc_handler);

static void soc_adc_handler(struct k_work *work)
{
	int16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};
	int32_t vbatt_mv;
	int soc;
	int err;

	adc_sequence_init_dt(&vbatt, &sequence);

	err = adc_read(vbatt.dev, &sequence);
	if (err < 0) {
		LOG_ERR("Could not read (%d)", err);
		return;
	}

	vbatt_mv = buf;
	adc_raw_to_millivolts_dt(&vbatt, &vbatt_mv);

	vbatt_mv = vbatt_mv *
		DT_PROP(VBATT_NODE, full_ohms) /
		DT_PROP(VBATT_NODE, output_ohms);

	soc = (vbatt_mv - VBATT_EMPTY_MV) * 100 /
		(VBATT_FULL_MV - VBATT_EMPTY_MV);
	soc = CLAMP(soc, 0, 100);

	LOG_INF("update_soc: vbatt_mv=%d soc=%d", vbatt_mv, soc);

	bt_bas_set_battery_level(soc);

	k_work_schedule(&soc_adc_dwork, K_SECONDS(SOC_UPDATE_SECS));
}

static int bas_soc_adc_init(void)
{
	int err;

	if (!device_is_ready(vbatt.dev)) {
		LOG_ERR("ADC controller device not ready");
		return -ENODEV;
	}

	err = adc_channel_setup_dt(&vbatt);
	if (err < 0) {
		LOG_ERR("Could not setup ADC channel (%d)", err);
		return err;
	}

	soc_adc_handler(&soc_adc_dwork.work);

	return 0;
}
SYS_INIT(bas_soc_adc_init, APPLICATION, 91);
