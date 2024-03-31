#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_hog.h"
#include "hid_kbd.h"
#include "hid_report_map.h"

LOG_MODULE_REGISTER(ble_hog, LOG_LEVEL_INF);

static struct hids_info info = {
	.version = 0x0000,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

static struct hids_report input_keyboard = {
	.id = HID_REPORT_ID_KBD,
	.type = HIDS_INPUT,
};

static bool notify_enabled;
static uint8_t ctrl_point;

static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, hid_report_map,
				 hid_report_map_len);
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	notify_enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_INF("notify_enabled: %d", notify_enabled);
}

static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_info, NULL, &info),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_report_map, NULL, NULL),

	/* Report 1: keyboard */
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ_ENCRYPT,
			       read_input_report, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,
		    BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &input_keyboard),

	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, write_ctrl_point, &ctrl_point),
);

static struct hid_kbd_report report_keyboard, report_keyboard_last;

static void input_cb(struct input_event *evt)
{
	hid_kbd_input_process(&report_keyboard, evt);

	if (!notify_enabled) {
		return;
	}

	if (!input_queue_empty()) {
		return;
	}

	if (memcmp(&report_keyboard_last, &report_keyboard, sizeof(report_keyboard))) {
		bt_gatt_notify(NULL, &hog_svc.attrs[5],
			       &report_keyboard, sizeof(report_keyboard));
		memcpy(&report_keyboard_last, &report_keyboard, sizeof(report_keyboard));
	}
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(keymap)), input_cb);
