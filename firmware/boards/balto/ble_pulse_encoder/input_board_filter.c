#define DT_DRV_COMPAT input_board_filter

#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(input_board_filter, CONFIG_INPUT_LOG_LEVEL);

#define SEQ_QUEUE_SZ 8

enum seq_code {
	SEQ_LEFT,
	SEQ_RIGHT,
	SEQ_P_5,
	SEQ_X,
	SEQ_GRAVE_U,
};

K_MSGQ_DEFINE(seq_msgq, sizeof(enum seq_code), SEQ_QUEUE_SZ, 4);

static void seq_thread(void *arg1, void *arg2, void *arg3)
{
	const struct device *dev = arg1;
	enum seq_code code;
	int ret;

	while (true) {
		ret = k_msgq_get(&seq_msgq, &code, K_FOREVER);
		if (ret) {
			LOG_ERR("k_msgq_get error: %d", ret);
			continue;
		}

		switch (code) {
		case SEQ_LEFT:
			input_report_key(dev, INPUT_KEY_LEFT, 1, true, K_FOREVER);
			k_sleep(K_MSEC(10));
			input_report_key(dev, INPUT_KEY_LEFT, 0, true, K_FOREVER);
			break;
		case SEQ_RIGHT:
			input_report_key(dev, INPUT_KEY_RIGHT, 1, true, K_FOREVER);
			k_sleep(K_MSEC(10));
			input_report_key(dev, INPUT_KEY_RIGHT, 0, true, K_FOREVER);
			break;
		case SEQ_P_5:
			input_report_key(dev, INPUT_KEY_P, 1, true, K_FOREVER);
			k_sleep(K_MSEC(10));
			input_report_key(dev, INPUT_KEY_P, 0, true, K_FOREVER);
			k_sleep(K_MSEC(200));
			input_report_key(dev, INPUT_KEY_5, 1, true, K_FOREVER);
			k_sleep(K_MSEC(10));
			input_report_key(dev, INPUT_KEY_5, 0, true, K_FOREVER);
			break;
		case SEQ_X:
			input_report_key(dev, INPUT_KEY_X, 1, true, K_FOREVER);
			k_sleep(K_MSEC(10));
			input_report_key(dev, INPUT_KEY_X, 0, true, K_FOREVER);
			break;
		case SEQ_GRAVE_U:
			input_report_key(dev, INPUT_KEY_GRAVE, 1, true, K_FOREVER);
			k_sleep(K_MSEC(10));
			input_report_key(dev, INPUT_KEY_GRAVE, 0, true, K_FOREVER);
			k_sleep(K_MSEC(200));
			input_report_key(dev, INPUT_KEY_U, 1, true, K_FOREVER);
			k_sleep(K_MSEC(10));
			input_report_key(dev, INPUT_KEY_U, 0, true, K_FOREVER);
			break;
		};

		k_sleep(K_MSEC(10));
	}
}

K_THREAD_DEFINE(sequencer, 1024, seq_thread, DEVICE_DT_INST_GET(0), NULL, NULL, 0, 0, 0);

static void board_filter_cb(struct input_event *evt, void *user_data)
{
	enum seq_code code;

	if (evt->type == INPUT_EV_REL && evt->code == INPUT_REL_WHEEL) {
		if (evt->value > 0) {
			code = SEQ_LEFT;
		} else {
			code = SEQ_RIGHT;
		}

		for (uint8_t i = 0; i < abs(evt->value); i++) {
			k_msgq_put(&seq_msgq, &code, K_NO_WAIT);
		}
	} else if (evt->type == INPUT_EV_KEY && evt->value == 1) {
		switch (evt->code) {
		case INPUT_BTN_RIGHT:
			code = SEQ_P_5;
			k_msgq_put(&seq_msgq, &code, K_NO_WAIT);
			break;
		case INPUT_KEY_X:
			code = SEQ_X;
			k_msgq_put(&seq_msgq, &code, K_NO_WAIT);
			break;
		case INPUT_KEY_U:
			code = SEQ_GRAVE_U;
			k_msgq_put(&seq_msgq, &code, K_NO_WAIT);
			break;
		default:
			break;
		}
	}
}

#define BOARD_FILTER_CB(node_id, prop, idx, n)						\
	INPUT_CALLBACK_DEFINE_NAMED(							\
			DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),		\
			board_filter_cb, (void *)DEVICE_DT_GET(node_id),		\
			board_filter_cb_##idx##_##n);

DT_INST_FOREACH_PROP_ELEM_VARGS(0, input, BOARD_FILTER_CB, n)

DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
		      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);
