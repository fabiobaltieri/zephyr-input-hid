#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

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
		BT_LE_ADV_OPT_CONNECTABLE,
		BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL);

#if CONFIG_SHELL

static int cmd_ble_unpair(const struct shell *sh, size_t argc, char **argv)
{
	int err;

	err = bt_unpair(BT_ID_DEFAULT, NULL);
	if (err) {
		shell_error(sh, "Failed to clear pairings (err %d)",
			    err);
		return err;
	}

	shell_print(sh, "Pairing successfully cleared");

	event(EVENT_BT_UNPAIRED);

	return 0;
}

static void connection_status(struct bt_conn *conn, void *user_data)
{
	const struct shell *sh = user_data;
	char addr[BT_ADDR_LE_STR_LEN];
	struct bt_conn_info info;
	int ret;

	ret = bt_conn_get_info(conn, &info);
	if (ret < 0) {
		shell_error(sh, "Unable to get info: conn=%p", conn);
		return;
	}

	bt_addr_le_to_str(info.le.dst, addr, sizeof(addr));

	shell_print(sh, "  [%u] %s %s: interval=%u latency=%u timeout=%u security=%d",
		    info.id,
		    info.role == BT_CONN_ROLE_CENTRAL ? "central" : "peripheral",
		    addr,
		    info.le.interval,
		    info.le.latency,
		    info.le.timeout,
		    info.security.level);
}

static void bond_status(const struct bt_bond_info *info, void *user_data)
{
	const struct shell *sh = user_data;
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(&info->addr, addr, sizeof(addr));
        shell_print(sh, "  %s", addr);
}

static int cmd_ble_status(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "Connections: max=%d", CONFIG_BT_MAX_CONN);
	bt_conn_foreach(BT_CONN_TYPE_LE, connection_status, (void *)sh);

	shell_print(sh, "Bonds: max=%d", CONFIG_BT_MAX_PAIRED);
	bt_foreach_bond(BT_ID_DEFAULT, bond_status, (void *)sh);

	return 0;
}
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ble_cmds,
	SHELL_CMD_ARG(unpair, NULL, "Unpair", cmd_ble_unpair, 0, 0),
	SHELL_CMD_ARG(status, NULL, "Connections", cmd_ble_status, 0, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ble, &sub_ble_cmds, "BLE commands", NULL);

#endif

static void ble_disconnect(struct bt_conn *conn, void *user_data)
{
	int err;

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
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

	err = bt_le_adv_start(&ad_param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return -EIO;
	}

	return 0;
}
SYS_INIT(ble_setup, APPLICATION, 99);
