#define DT_DRV_COMPAT ble_unpair

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "event.h"

LOG_MODULE_REGISTER(ble_unpair, LOG_LEVEL_INF);

#define UNPAIR_TIMEOUT_S 5

#define KEY_A DT_INST_PROP(0, key_a)
#define KEY_B DT_INST_PROP(0, key_b)

static void unpair_handler(struct k_work *work)
{
	bt_unpair(BT_ID_DEFAULT, NULL);

	event(EVENT_BT_UNPAIRED);
}

K_WORK_DELAYABLE_DEFINE(unpair_dwork, unpair_handler);

static void unpair_cb(struct input_event *evt)
{
	static bool a, b;

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	if (evt->code == KEY_A) {
		a = evt->value;
	}
	else if (evt->code == KEY_B) {
		b = evt->value;
	}

	if (a && b) {
		k_work_schedule(&unpair_dwork, K_SECONDS(UNPAIR_TIMEOUT_S));
	} else {
		k_work_cancel_delayable(&unpair_dwork);
	}
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET_OR_NULL(DT_INST_PHANDLE(0, input)), unpair_cb);
