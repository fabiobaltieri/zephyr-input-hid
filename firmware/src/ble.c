#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "event.h"

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      CONFIG_BT_DEVICE_APPEARANCE & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static struct bt_le_adv_param ad_param = BT_LE_ADV_PARAM_INIT(
		BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,
		BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL);

static void connected(struct bt_conn *conn, uint8_t err)
{
	int ret;

	ret = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (ret) {
		LOG_WRN("Failed to set security");
	}

	event(EVENT_BT_CONNECTED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	event(EVENT_BT_DISCONNECTED);
}

static void adv_work_handler(struct k_work *work)
{
	int ret;

	ret = bt_le_adv_start(&ad_param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (ret) {
		LOG_ERR("bt_le_adv_start failed: %d", ret);
		return;
	}
}
K_WORK_DEFINE(adv_work, adv_work_handler);

static void recycled(void)
{
	k_work_submit(&adv_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

static void ble_disconnect(struct bt_conn *conn, void *user_data)
{
	int err;

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_POWER_OFF);
	if (err) {
		LOG_ERR("bt_conn_disconnect: %d", err);
	}
}

void ble_stop(void)
{
	int err;

	err = bt_le_adv_stop();
	if (err) {
		LOG_ERR("bt_le_adv_stop: %d", err);
	}

	bt_conn_foreach(BT_CONN_TYPE_LE, ble_disconnect, NULL);
}

static int ble_setup(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return -EIO;
	}

	LOG_INF("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	k_work_submit(&adv_work);

	return 0;
}
SYS_INIT(ble_setup, APPLICATION, 99);
