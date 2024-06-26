#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include "blinker.h"

LOG_MODULE_REGISTER(ble_state, LOG_LEVEL_INF);

static const struct device *sensor = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(sensor));

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (sensor != NULL) {
		pm_device_action_run(sensor, PM_DEVICE_ACTION_RESUME);
	}

	blink(BLINK_CONNECTED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (sensor != NULL) {
		pm_device_action_run(sensor, PM_DEVICE_ACTION_SUSPEND);
	}

	blink(BLINK_DISCONNECTED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};
