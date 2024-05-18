#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "hid.h"
#include "hid_kbd.h"

LOG_MODULE_REGISTER(ble_hog, LOG_LEVEL_INF);

enum {
	HIDS_REMOTE_WAKE = BIT(0),
	HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
	uint16_t version;
	uint8_t code;
	uint8_t flags;
} __packed;

struct hids_report {
	uint8_t id; /* report id */
	uint8_t type; /* report type */
} __packed;

enum {
	HIDS_INPUT = 0x01,
	HIDS_OUTPUT = 0x02,
	HIDS_FEATURE = 0x03,
};

static struct hids_info info = {
	.version = 0x0000,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
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
	const struct device *dev = attr->user_data;
	const uint8_t *data = hid_dev_report(dev);
	uint16_t data_len = hid_dev_report_len(dev);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, data, data_len);
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

static __maybe_unused ssize_t write_output_report(
		struct bt_conn *conn,
		const struct bt_gatt_attr *attr,
		const void *buf,
		uint16_t len, uint16_t offset, uint8_t flags)
{
	return len;
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

#define HIDS_REPORT_DEFINE_INPUT(node_id, prop, idx)	\
	(&(const struct hids_report){			\
	 .id = DT_PROP_BY_IDX(node_id, prop, idx),	\
	 .type = HIDS_INPUT,				\
	 })

#define HIDS_REPORT_DEFINE_OUTPUT(node_id, prop, idx)	\
	(&(const struct hids_report){			\
	 .id = DT_PROP_BY_IDX(node_id, prop, idx),	\
	 .type = HIDS_INPUT,				\
	 })

#define HOG_DEVICE_DEFINE_INPUT(node_id, prop, idx)					\
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,					\
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,			\
			       BT_GATT_PERM_READ_ENCRYPT,				\
			       read_input_report, NULL, NULL),				\
	BT_GATT_CCC(input_ccc_changed,							\
		    BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),		\
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,			\
			   read_report, NULL,						\
			   (void *)HIDS_REPORT_DEFINE_INPUT(node_id, prop, idx)),

#define HOG_DEVICE_DEFINE_OUTPUT(node_id, prop, idx)					\
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,					\
			       BT_GATT_CHRC_WRITE,					\
			       BT_GATT_PERM_WRITE_ENCRYPT,				\
			       NULL, write_output_report, NULL),			\
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,			\
			   read_report, NULL,						\
			   (void *)HIDS_REPORT_DEFINE_OUTPUT(node_id, prop, idx)),

#define HOG_DEVICE_DEFINE(node_id)						\
	IF_ENABLED(DT_NODE_HAS_PROP(node_id, input_id), (			\
	DT_FOREACH_PROP_ELEM(node_id, input_id, HOG_DEVICE_DEFINE_INPUT)	\
	))									\
	IF_ENABLED(DT_NODE_HAS_PROP(node_id, output_id), (			\
	DT_FOREACH_PROP_ELEM(node_id, output_id, HOG_DEVICE_DEFINE_OUTPUT)	\
	))

#define HOG_SVC_DEFINE(node_id)								\
	BT_GATT_SERVICE_DEFINE(hog_svc_##node_id,					\
		BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),					\
		BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,		\
				       BT_GATT_PERM_READ, read_info, NULL, &info),	\
		BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,	\
				       BT_GATT_PERM_READ, read_report_map, NULL,	\
				       (void *)DEVICE_DT_GET(node_id)),			\
											\
		DT_FOREACH_CHILD(node_id, HOG_DEVICE_DEFINE)				\
											\
		BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,				\
				       BT_GATT_CHRC_WRITE_WITHOUT_RESP,			\
				       BT_GATT_PERM_WRITE,				\
				       NULL, write_ctrl_point, &ctrl_point),		\
	);

DT_FOREACH_STATUS_OKAY(hid, HOG_SVC_DEFINE)

#define HID_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(hid)
#define SVC_NAME _CONCAT(hog_svc_, HID_NODE)

static const struct bt_gatt_attr *kbd_report_attr = &SVC_NAME.attrs[5];

static struct {
#if CONFIG_KBD_HID_NKRO
	struct hid_kbd_report_nkro curr;
	struct hid_kbd_report_nkro last;
#else
	struct hid_kbd_report curr;
	struct hid_kbd_report last;
	struct hid_kbd_report_data data;
#endif
	bool pending;
	struct k_sem lock;
} report;

static int ble_hog_init(void)
{
	k_sem_init(&report.lock, 1, 1);

	return 0;
}
SYS_INIT(ble_hog_init, APPLICATION, 0);

static void ble_hog_notify_cb(struct bt_conn *conn, void *user_data)
{
	void (*cb)(void) = user_data;

	k_sem_take(&report.lock, K_FOREVER);
	report.pending = false;
	k_sem_give(&report.lock);

	cb();
}

static void hog_send_report(void)
{
	struct bt_gatt_notify_params params;
	int ret;

	k_sem_take(&report.lock, K_FOREVER);

	if (memcmp(&report.last, &report.curr, sizeof(report.curr)) == 0) {
		goto out;
	}

	if (report.pending) {
		goto out;
	}

	report.pending = true;

	memset(&params, 0, sizeof(params));
	params.attr = kbd_report_attr;
	params.data = &report.curr;
	params.len = sizeof(report.curr);
	params.func = ble_hog_notify_cb;
	params.user_data = hog_send_report;

	ret = bt_gatt_notify_cb(NULL, &params);
	if (ret) {
		LOG_WRN("bt_gatt_notify_cb failed: %d", ret);
		report.pending = false;
	}

	memcpy(&report.last, &report.curr, sizeof(report.curr));

out:
	k_sem_give(&report.lock);
}

static void hog_input_cb(struct input_event *evt)
{
	k_sem_take(&report.lock, K_FOREVER);
#ifdef CONFIG_KBD_HID_NKRO
	hid_kbd_input_process_nkro(&report.curr, evt);
#else
	hid_kbd_input_process(&report.curr, &report.data, evt);
#endif
	k_sem_give(&report.lock);

	if (!notify_enabled) {
		return;
	}

	if (!input_queue_empty()) {
		return;
	}

	hog_send_report();
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET_OR_NULL(DT_NODELABEL(keymap)), hog_input_cb);
