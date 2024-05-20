#define DT_DRV_COMPAT hid_hog

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "hid.h"

LOG_MODULE_REGISTER(ble_hog, LOG_LEVEL_INF);

#define HOG_REPORT_BUF_SIZE 32

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
	uint8_t id;
	uint8_t type;
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
	const uint8_t *data = hid_report(dev);
	uint16_t data_len = hid_report_len(dev);

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
	 .type = HIDS_OUTPUT,				\
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
		DT_FOREACH_CHILD(DT_CHILD(node_id, input), HOG_DEVICE_DEFINE)				\
											\
		BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,				\
				       BT_GATT_CHRC_WRITE_WITHOUT_RESP,			\
				       BT_GATT_PERM_WRITE,				\
				       NULL, write_ctrl_point, &ctrl_point),		\
	);

DT_FOREACH_STATUS_OKAY(hid, HOG_SVC_DEFINE)

#define HOG_SVC_ENTRY_DEFINE(node_id) {	\
	.dev = DEVICE_DT_GET(node_id),	\
	.svc = &hog_svc_##node_id,	\
},

static const struct {
	const struct device *dev;
	const struct bt_gatt_service_static *svc;
} hog_svc[] = {
	DT_FOREACH_STATUS_OKAY(hid, HOG_SVC_ENTRY_DEFINE)
};

static const uint8_t hog_svc_count = ARRAY_SIZE(hog_svc);

#define HID_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(hid)
#define SVC_NAME _CONCAT(hog_svc_, HID_NODE)

static const struct device *ble_hog_dev = DEVICE_DT_INST_GET(0);

static bool hog_busy;

static void ble_hog_notify_cb(struct bt_conn *conn, void *user_data)
{
	void (*cb)(void) = user_data;

	hog_busy = false;

	cb();
}

static const struct bt_gatt_attr *hog_find_attr(const struct bt_gatt_service_static *svc,
						uint8_t report_id)
{
	const struct bt_gatt_attr *out = NULL;

	for (size_t i = 0; i < svc->attr_count; i++) {
		const struct bt_gatt_attr *attr = &svc->attrs[i];

		if (attr->read == bt_gatt_attr_read_chrc) {
			out = attr;
		} else if (attr->read == read_report) {
			struct hids_report *data = attr->user_data;

			if (data->id == report_id && data->type == HIDS_INPUT) {
				return out;
			}
		}
	}

	return NULL;
}

static void hog_send_report(void)
{
	struct bt_gatt_notify_params params;
	const struct bt_gatt_attr *attr;
	int ret;
	uint8_t report_id;
	uint8_t buf[HOG_REPORT_BUF_SIZE];
	int size;

	if (hog_busy) {
		return;
	}

	for (uint8_t i = 0; i < hog_svc_count; i++) {
		const struct device *hid_dev = hog_svc[i].dev;

		if (!hid_has_updates(hid_dev, ble_hog_dev)) {
			continue;
		}

		size = hid_get_report(hid_dev, ble_hog_dev, &report_id, buf, sizeof(buf));
		if (size < 0) {
			LOG_ERR("get_report error: %d", size);
			continue;
		}

		hog_busy = true;

		attr = hog_find_attr(hog_svc[i].svc, report_id);
		if (attr == NULL) {
			LOG_ERR("hog_find_attr error: %d", size);
			continue;
		}

		memset(&params, 0, sizeof(params));
		params.attr = attr;
		params.data = buf;
		params.len = size;
		params.func = ble_hog_notify_cb;
		params.user_data = hog_send_report;

		ret = bt_gatt_notify_cb(NULL, &params);
		if (ret) {
			LOG_WRN("bt_gatt_notify_cb failed: %d", ret);
			hog_busy = false;
		}

		return;
	}
}

static void ble_hog_notify(const struct device *dev)
{
	if (!notify_enabled) {
		return;
	}

	hog_send_report();
}

static const struct hid_output_api ble_hog_api = {
	.notify = ble_hog_notify,
};

DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		      &ble_hog_api);
