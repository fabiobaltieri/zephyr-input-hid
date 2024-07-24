#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT button_scan

LOG_MODULE_REGISTER(button_scan, LOG_LEVEL_INF);

DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
                      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

static K_TIMER_DEFINE(button_scan_sync, NULL, NULL);

#define HOLDOFF_TIME_MS 100

#define INPUT_DELAY_US DT_INST_PROP(0, input_delay_us)
#define GROUP_DELAY_MS DT_INST_PROP(0, group_delay_ms)

#define GROUP1_COUNT DT_INST_PROP_LEN(0, group1_gpios)
#define GROUP2_COUNT DT_INST_PROP_LEN(0, group2_gpios)

#define BUTTONS_INIT(node, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx),

#define BUTTONS_CODE(node, prop, idx) DT_PROP_BY_IDX(node, prop, idx),

#define GROUP_PULSE(prop) \
	(DT_INST_PWMS_PERIOD(0) * DT_INST_PROP(0, prop) / 100)

static const struct {
	const struct gpio_dt_spec input_gpio;
	const struct gpio_dt_spec group1_gpios[GROUP1_COUNT];
	const struct gpio_dt_spec group2_gpios[GROUP2_COUNT];
	const uint16_t group1_codes[GROUP1_COUNT];
	const uint16_t group2_codes[GROUP2_COUNT];
	const struct pwm_dt_spec ref_pwm;
	uint32_t pulse_group1;
	uint32_t pulse_group2;
} cfg = {
	.group1_gpios = {
		DT_INST_FOREACH_PROP_ELEM(0, group1_gpios, BUTTONS_INIT)
	},
	.group2_gpios = {
		DT_INST_FOREACH_PROP_ELEM(0, group2_gpios, BUTTONS_INIT)
	},
	.group1_codes = {
		DT_INST_FOREACH_PROP_ELEM(0, codes_group1, BUTTONS_CODE)
	},
	.group2_codes = {
		DT_INST_FOREACH_PROP_ELEM(0, codes_group2, BUTTONS_CODE)
	},
	.input_gpio = GPIO_DT_SPEC_INST_GET(0, input_gpios),
	.ref_pwm = PWM_DT_SPEC_INST_GET(0),
	.pulse_group1 = GROUP_PULSE(pulse_group1_pct),
	.pulse_group2 = GROUP_PULSE(pulse_group2_pct),
};

static struct {
	uint16_t state;
	uint32_t group1_last_update[GROUP1_COUNT];
	uint32_t group2_last_update[GROUP2_COUNT];
} data;

static void button_scan_group(const struct gpio_dt_spec *gpios,
			      const uint16_t *codes,
			      uint32_t *last_update,
			      int group_size, int offset)
{
	const struct device *dev = DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT));
	uint16_t old_state;
	bool val;
	int i, j;
	uint32_t time_now;

	time_now = k_uptime_get_32();

	for (i = 0; i < group_size; i++) {
		if (time_now - last_update[i] < HOLDOFF_TIME_MS) {
			continue;
		}

		j = i + offset;
		gpio_pin_configure_dt(&gpios[i], GPIO_OUTPUT_HIGH);

		k_busy_wait(INPUT_DELAY_US);

		old_state = data.state;

		val = gpio_pin_get_dt(&cfg.input_gpio);
		WRITE_BIT(data.state, j, val);

		if (old_state != data.state) {
			input_report_key(dev, codes[i], val, true, K_FOREVER);
			last_update[i] = time_now;
			LOG_DBG("button_scan[%d]: %d", j, val);
		}

		gpio_pin_configure_dt(&gpios[i], GPIO_INPUT);
	}
}

static void button_scan_loop(void)
{
	button_scan_group(cfg.group1_gpios, cfg.group1_codes, data.group1_last_update,
			  GROUP1_COUNT, 0);

	pwm_set_pulse_dt(&cfg.ref_pwm, cfg.pulse_group2);
	k_sleep(K_MSEC(GROUP_DELAY_MS));

	button_scan_group(cfg.group2_gpios, cfg.group2_codes, data.group2_last_update,
			  GROUP2_COUNT, GROUP1_COUNT);

	pwm_set_pulse_dt(&cfg.ref_pwm, cfg.pulse_group1);
}

static void button_scan_thread(void)
{
	int i;
	int ret;

	for (i = 0; i < GROUP1_COUNT; i++) {
		ret = gpio_pin_configure_dt(&cfg.group1_gpios[i], GPIO_INPUT);
		if (ret) {
			LOG_ERR("gpio_pin_configure_dt failed: %d", ret);
			return;
		}
	}

	for (i = 0; i < GROUP2_COUNT; i++) {
		ret = gpio_pin_configure_dt(&cfg.group2_gpios[i], GPIO_INPUT);
		if (ret) {
			LOG_ERR("gpio_pin_configure_dt failed: %d", ret);
			return;
		}
	}

	ret = gpio_pin_configure_dt(&cfg.input_gpio, GPIO_INPUT);
	if (ret) {
		LOG_ERR("gpio_pin_configure_dt failed: %d", ret);
		return;
	}

	pwm_set_pulse_dt(&cfg.ref_pwm, cfg.pulse_group1);

	k_timer_start(&button_scan_sync, K_MSEC(15), K_MSEC(15));

	while (true) {
		button_scan_loop();
		k_timer_status_sync(&button_scan_sync);
	}
}

K_THREAD_DEFINE(button_scan, 512, button_scan_thread, NULL, NULL, NULL, 7, 0, 0);
