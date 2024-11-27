#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>

#include "ble_hog.h"

#if CONFIG_SHELL
extern const struct Z_DEVICE_API_TYPE(hid_output) ble_hog_api;

static bool hog_device_filter(const struct device *dev)
{
	return dev->api == &ble_hog_api;
}

static void device_name_get(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_filter(idx, hog_device_filter);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(dsub_device_name, device_name_get);

struct search_conn_arg {
	const struct shell *sh;
	int id;
	int search_id;
	bool found;
	bt_addr_le_t peer;
};

static void search_conn_id(struct bt_conn *conn, void *user_data)
{
	struct search_conn_arg *arg = user_data;
	struct bt_conn_info info;
	int ret;

	arg->search_id++;
	if (arg->id != arg->search_id) {
		return;
	}

	ret = bt_conn_get_info(conn, &info);
	if (ret < 0) {
		shell_error(arg->sh, "Unable to get info: conn=%p", conn);
		return;
	}

	memcpy(&arg->peer, info.le.dst, sizeof(bt_addr_le_t));
	arg->found = true;
}

static int cmd_hog_peer(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	struct search_conn_arg arg;
	char addr[BT_ADDR_LE_STR_LEN];
	int err = 0;

	arg.sh = sh;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Device %s not available", argv[1]);
		return -ENODEV;
	}

	arg.id = shell_strtol(argv[2], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[2]);
		return err;
	}

	if (arg.id < 0) {
		shell_print(sh, "ble_hog_set_peer: %s -> NULL", dev->name);
		ble_hog_set_peer(dev, NULL);
		return 0;
	}

	arg.found = false;
	arg.search_id = -1;
	bt_conn_foreach(BT_CONN_TYPE_LE, search_conn_id, &arg);

	if (!arg.found) {
		shell_error(sh, "No peer found for id %d", arg.id);
		return -EINVAL;
	}

	bt_addr_le_to_str(&arg.peer, addr, sizeof(addr));
	shell_print(sh, "ble_hog_set_peer: %s -> %s", dev->name, addr);

	ble_hog_set_peer(dev, &arg.peer);

	return 0;
}
SHELL_CMD_ARG_REGISTER(hog_peer, &dsub_device_name, "Set HOG peer", cmd_hog_peer, 3, 0);
#endif
