#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/shell/shell.h>

#include "event.h"

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

	shell_print(sh, "  %s %s: interval=%u latency=%u timeout=%u security=%d",
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

