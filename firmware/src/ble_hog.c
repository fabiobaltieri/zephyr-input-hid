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

struct ble_hog_config {
	const struct device *hid_dev;
	const struct bt_gatt_service_static *svc;
};

struct ble_hog_data {
	bool hog_busy;
};

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

#define HOG_SVC_DEFINE(n)								\
	BT_GATT_SERVICE_DEFINE(ble_hog_svc_##n,						\
		BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),					\
		BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,		\
				       BT_GATT_PERM_READ, read_info, NULL, &info),	\
		BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,	\
				       BT_GATT_PERM_READ, read_report_map, NULL,	\
				       (void *)DEVICE_DT_GET(DT_INST_GPARENT(n))),	\
											\
		DT_FOREACH_CHILD(DT_CHILD(DT_INST_GPARENT(n), input),			\
				 HOG_DEVICE_DEFINE)					\
											\
		BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,				\
				       BT_GATT_CHRC_WRITE_WITHOUT_RESP,			\
				       BT_GATT_PERM_WRITE,				\
				       NULL, write_ctrl_point, &ctrl_point),		\
	);

static void ble_hog_notify_cb(struct bt_conn *conn, void *user_data)
{
	const struct device *dev = user_data;
	struct ble_hog_data *data = dev->data;

	data->hog_busy = false;

	hid_output_notify(dev);
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

static void ble_hog_notify(const struct device *dev)
{
	const struct ble_hog_config *cfg = dev->config;
	struct ble_hog_data *data = dev->data;
	struct bt_gatt_notify_params params;
	const struct bt_gatt_attr *attr;
	int ret;
	uint8_t report_id;
	uint8_t buf[HOG_REPORT_BUF_SIZE];
	int size;

	if (data->hog_busy) {
		return;
	}

	if (!hid_has_updates(cfg->hid_dev, dev)) {
		return;
	}

	size = hid_get_report(cfg->hid_dev, dev, &report_id, buf, sizeof(buf));
	if (size < 0) {
		LOG_ERR("get_report error: %d", size);
		return;
	}

	if (!notify_enabled) {
		return;
	}

	data->hog_busy = true;

	attr = hog_find_attr(cfg->svc, report_id);
	if (attr == NULL) {
		LOG_ERR("hog_find_attr error: %d", size);
		return;
	}

	memset(&params, 0, sizeof(params));
	params.attr = attr;
	params.data = buf;
	params.len = size;
	params.func = ble_hog_notify_cb;
	params.user_data = (void *)dev;

	ret = bt_gatt_notify_cb(NULL, &params);
	if (ret) {
		LOG_WRN("bt_gatt_notify_cb failed: %d", ret);
		data->hog_busy = false;
	}
}

static int ble_hog_init(const struct device *dev)
{
	return 0;
}

static const struct hid_output_api ble_hog_api = {
	.notify = ble_hog_notify,
};

#define BLE_HOG_INIT(n)							\
	HOG_SVC_DEFINE(n)						\
									\
	static const struct ble_hog_config ble_hog_cfg_##n = {		\
		.hid_dev = DEVICE_DT_GET(DT_INST_GPARENT(n)),		\
		.svc = &ble_hog_svc_##n,				\
	};								\
									\
	static struct ble_hog_data ble_hog_data_##n;			\
									\
DEVICE_DT_INST_DEFINE(n, ble_hog_init, NULL,				\
		      &ble_hog_data_##n, &ble_hog_cfg_##n,		\
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,	\
		      &ble_hog_api);

DT_INST_FOREACH_STATUS_OKAY(BLE_HOG_INIT)
