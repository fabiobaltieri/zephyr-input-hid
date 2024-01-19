#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "ble_hog.h"

LOG_MODULE_REGISTER(ble_hog, LOG_LEVEL_INF);

static struct hids_info info = {
	.version = 0x0000,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

static struct hids_report input_keyboard = {
	.id = 0x01,
	.type = HIDS_INPUT,
};

static bool notify_enabled;
static uint8_t ctrl_point;
static uint8_t report_map[] = {
	0x05, 0x01,                    // Usage Page (Generic Desktop)
	0x09, 0x06,                    // Usage (Keyboard)
	0xa1, 0x01,                    // Collection (Application)
	0x85, 0x01,                    //  Report ID (1)
	0x05, 0x07,                    //  Usage Page (Keyboard)
	0x19, 0xe0,                    //  Usage Minimum (224)
	0x29, 0xe7,                    //  Usage Maximum (231)
	0x15, 0x00,                    //  Logical Minimum (0)
	0x25, 0x01,                    //  Logical Maximum (1)
	0x95, 0x08,                    //  Report Count (8)
	0x75, 0x01,                    //  Report Size (1)
	0x81, 0x02,                    //  Input (Data,Var,Abs)
	0x95, 0x01,                    //  Report Count (1)
	0x75, 0x08,                    //  Report Size (8)
	0x81, 0x01,                    //  Input (Cnst,Arr,Abs)
	0x05, 0x07,                    //  Usage Page (Keyboard)
	0x19, 0x00,                    //  Usage Minimum (0)
	0x2a, 0xff, 0x00,              //  Usage Maximum (255)
	0x15, 0x00,                    //  Logical Minimum (0)
	0x26, 0xff, 0x00,              //  Logical Maximum (255)
	0x95, 0x06,                    //  Report Count (6)
	0x75, 0x08,                    //  Report Size (8)
	0x81, 0x00,                    //  Input (Data,Arr,Abs)
	0xc0,                          // End Collection
};

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
	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
				 sizeof(report_map));
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

static int code_to_bit(const struct code_to_bit_map *map, int map_size,
		       uint16_t code)
{
	int i;

	for (i = 0; i < map_size; i++) {
		if (map[i].code == code) {
			return map[i].bit;
		}
	}
	return -1;
}

#define KEYS_REPORT_SIZE 6
static struct {
	uint8_t modifiers;
	uint8_t _reserved;
	uint8_t keys[KEYS_REPORT_SIZE];
} __packed report_keyboard, report_keyboard_last;

static const struct code_to_bit_map button_map_keyboard[] = {
	{INPUT_KEY_A, 0x04},
	{INPUT_KEY_B, 0x05},
	{INPUT_KEY_C, 0x06},
	{INPUT_KEY_D, 0x07},
	{INPUT_KEY_E, 0x08},
	{INPUT_KEY_F, 0x09},
	{INPUT_KEY_G, 0x0a},
	{INPUT_KEY_H, 0x0b},
	{INPUT_KEY_I, 0x0c},
};

static void ble_hog_set_key_keyboard(uint16_t code, uint32_t value)
{
	int i;
	int hid_code;

	/* modifiers */
	if (code == INPUT_KEY_LEFTALT) {
		WRITE_BIT(report_keyboard.modifiers, 5, value);
		return;
	} else if (code == INPUT_KEY_LEFTSHIFT) {
		WRITE_BIT(report_keyboard.modifiers, 6, value);
		return;
	} else if (code == INPUT_KEY_LEFTCTRL) {
		WRITE_BIT(report_keyboard.modifiers, 7, value);
		return;
	}

	/* normal keys */
	hid_code = code_to_bit(button_map_keyboard,
			       ARRAY_SIZE(button_map_keyboard),
			       code);
	if (hid_code < 0) {
		return;
	}

	if (value) {
		for (i = 0; i < KEYS_REPORT_SIZE; i++) {
			if (report_keyboard.keys[i] == 0x00) {
				report_keyboard.keys[i] = hid_code;
				return;
			}
		}
	} else {
		for (i = 0; i < KEYS_REPORT_SIZE; i++) {
			if (report_keyboard.keys[i] == hid_code) {
				report_keyboard.keys[i] = 0x00;
				return;
			}
		}
	}
}

static void input_cb(struct input_event *evt)
{
	if (evt->type == INPUT_EV_KEY) {
		ble_hog_set_key_keyboard(evt->code, evt->value);
	} else {
		LOG_ERR("unrecognized event type: %x", evt->type);
	}

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
