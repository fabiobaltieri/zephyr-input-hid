#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "blinker.h"

LOG_MODULE_REGISTER(ble_unpair, LOG_LEVEL_INF);

#define UNPAIR_TIMEOUT_S 5

static void unpair_handler(struct k_work *work)
{
	bt_unpair(BT_ID_DEFAULT, NULL);

	blink(BLINK_UNPAIRED);
}

K_WORK_DELAYABLE_DEFINE(unpair_dwork, unpair_handler);

static void unpair_cb(struct input_event *evt)
{
	static bool a, b;

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	if (evt->code == INPUT_BTN_LEFT) {
		a = evt->value;
	}
	else if (evt->code == INPUT_BTN_RIGHT) {
		b = evt->value;
	}

	if (a && b) {
		k_work_schedule(&unpair_dwork, K_SECONDS(UNPAIR_TIMEOUT_S));
	} else {
		k_work_cancel_delayable(&unpair_dwork);
	}
}
INPUT_CALLBACK_DEFINE(NULL, unpair_cb);
