#define DT_DRV_COMPAT hid_hog

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>

#include "ble_hog.h"
#include "hid.h"

LOG_MODULE_REGISTER(ble_hog, LOG_LEVEL_INF);

struct ble_hog_config {
	const struct device *hid_dev;
	const struct bt_gatt_service_static *svc;
};

struct ble_hog_data {
	uint32_t notify_mask;
	bool hog_busy;
	bool suspended;
	bool peer_set;
	bt_addr_le_t peer;
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
	const struct device *dev;
	struct {
		uint8_t id;
		uint8_t type;
	} report;
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

static uint8_t ctrl_point;

void ble_hog_set_peer(const struct device *dev, const bt_addr_le_t *peer)
{
	struct ble_hog_data *data = dev->data;

	if (peer == NULL) {
		data->peer_set = false;
		return;
	}

	memcpy(&data->peer, peer, sizeof(*peer));
	data->peer_set = true;
}

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
	const struct hids_report *hids_report = attr->user_data;

	LOG_DBG("read_report %p %d", &hids_report->report, sizeof(hids_report->report));

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &hids_report->report,
				 sizeof(hids_report->report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	const struct bt_gatt_attr *next_attr = attr + 1;

	if (next_attr->read != read_report) {
		LOG_ERR("invalid next_attr: %p", next_attr);
		return;
	}

	const struct hids_report *hids_report = next_attr->user_data;
	const struct device *dev = hids_report->dev;
	struct ble_hog_data *data = dev->data;
	bool enabled = value == BT_GATT_CCC_NOTIFY;

	WRITE_BIT(data->notify_mask, hids_report->report.id, enabled);

	LOG_INF("notify_enabled: dev=%s report_id=%d enabled=%d", dev->name, hids_report->report.id, enabled);
}

struct read_input_report_data {
	const struct device *dev;
	uint8_t id;
};

static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	const struct read_input_report_data *data = attr->user_data;
	const struct device *dev = data->dev;
	const struct ble_hog_config *cfg = dev->config;
	uint8_t report_buf[HOG_REPORT_BUF_SIZE];
	int size;

	if (offset != 0) {
		LOG_ERR("unsupported offset: %d", offset);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	size = hid_get_report_id(cfg->hid_dev, dev, data->id, report_buf, HOG_REPORT_BUF_SIZE);
	if (size < 0) {
		LOG_WRN("hid_get_report error: %d", size);
		return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_buf, size);
}

static __maybe_unused ssize_t write_output_report(
		struct bt_conn *conn,
		const struct bt_gatt_attr *attr,
		const void *buf,
		uint16_t len, uint16_t offset, uint8_t flags)
{
	const struct bt_gatt_attr *next_attr = attr + 1;

	if (next_attr->read != read_report) {
		LOG_ERR("invalid next_attr: %p", next_attr);
		return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
	}

	const struct hids_report *hids_report = next_attr->user_data;
	const struct device *dev = hids_report->dev;
	const struct ble_hog_config *cfg = dev->config;

	if (offset != 0) {
		LOG_ERR("unsupported offset: %d", offset);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	hid_out_report(cfg->hid_dev, hids_report->report.id, buf, len);

	return len;
}

static __maybe_unused ssize_t write_feature_report(
		struct bt_conn *conn,
		const struct bt_gatt_attr *attr,
		const void *buf,
		uint16_t len, uint16_t offset, uint8_t flags)
{
	const struct bt_gatt_attr *next_attr = attr + 1;

	if (next_attr->read != read_report) {
		LOG_ERR("invalid next_attr: %p", next_attr);
		return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
	}

	const struct hids_report *hids_report = next_attr->user_data;
	const struct device *dev = hids_report->dev;
	const struct ble_hog_config *cfg = dev->config;
	int ret;

	if (offset != 0) {
		LOG_ERR("unsupported offset: %d", offset);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	ret = hid_set_feature(cfg->hid_dev, hids_report->report.id, buf, len);
	if (ret < 0) {
		LOG_WRN("hid_out_report error: %d", ret);
		return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
	}

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

#define READ_INPUT_REPORT_DATA_DEFINE(node_id, prop, idx, self)	\
	(&(const struct read_input_report_data){		\
	 .dev = self,						\
	 .id = DT_PROP_BY_IDX(node_id, prop, idx),		\
	 })

#define HIDS_REPORT_DEFINE_INPUT(node_id, prop, idx, self)	\
	(&(const struct hids_report){				\
	 .dev = self,						\
	 .report.id = DT_PROP_BY_IDX(node_id, prop, idx),	\
	 .report.type = HIDS_INPUT,				\
	 })

#define HIDS_REPORT_DEFINE_OUTPUT(node_id, prop, idx, self)	\
	(&(const struct hids_report){				\
	 .dev = self,						\
	 .report.id = DT_PROP_BY_IDX(node_id, prop, idx),	\
	 .report.type = HIDS_OUTPUT,				\
	 })

#define HIDS_REPORT_DEFINE_FEATURE(node_id, prop, idx, self)	\
	(&(const struct hids_report){				\
	 .dev = self,						\
	 .report.id = DT_PROP_BY_IDX(node_id, prop, idx),	\
	 .report.type = HIDS_FEATURE,				\
	 })

#define HOG_DEVICE_DEFINE_INPUT(node_id, prop, idx, self)				\
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,					\
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,			\
			       BT_GATT_PERM_READ_ENCRYPT,				\
			       read_input_report, NULL,					\
			       (void *)READ_INPUT_REPORT_DATA_DEFINE(			\
					node_id, prop, idx, self)),			\
	BT_GATT_CCC(input_ccc_changed,							\
		    BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),		\
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,			\
			   read_report, NULL,						\
			   (void *)HIDS_REPORT_DEFINE_INPUT(node_id, prop, idx, self)),

#define HOG_DEVICE_DEFINE_OUTPUT(node_id, prop, idx, self)				\
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,					\
			       BT_GATT_CHRC_WRITE,					\
			       BT_GATT_PERM_WRITE_ENCRYPT,				\
			       NULL, write_output_report, NULL),			\
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,			\
			   read_report, NULL,						\
			   (void *)HIDS_REPORT_DEFINE_OUTPUT(node_id, prop, idx, self)),

#define HOG_DEVICE_DEFINE_FEATURE(node_id, prop, idx, self)				\
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,					\
			       BT_GATT_CHRC_WRITE,					\
			       BT_GATT_PERM_WRITE_ENCRYPT,				\
			       NULL, write_feature_report, NULL),			\
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,			\
			   read_report, NULL,						\
			   (void *)HIDS_REPORT_DEFINE_FEATURE(node_id, prop, idx, self)),

#define HOG_DEVICE_DEFINE(node_id, self)							\
	IF_ENABLED(DT_NODE_HAS_PROP(node_id, input_id), (					\
	DT_FOREACH_PROP_ELEM_VARGS(node_id, input_id, HOG_DEVICE_DEFINE_INPUT, self)		\
	))											\
	IF_ENABLED(DT_NODE_HAS_PROP(node_id, output_id), (					\
	DT_FOREACH_PROP_ELEM_VARGS(node_id, output_id, HOG_DEVICE_DEFINE_OUTPUT, self)		\
	))											\
	IF_ENABLED(DT_NODE_HAS_PROP(node_id, feature_id), (					\
	DT_FOREACH_PROP_ELEM_VARGS(node_id, feature_id, HOG_DEVICE_DEFINE_FEATURE, self)	\
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
		DT_FOREACH_CHILD_VARGS(DT_CHILD(DT_INST_GPARENT(n), input),		\
				       HOG_DEVICE_DEFINE, DEVICE_DT_INST_GET(n))	\
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

static const struct bt_gatt_attr *hog_find_attr(const struct device *dev,
						uint8_t report_id)
{
	const struct ble_hog_config *cfg = dev->config;
	const struct bt_gatt_service_static *svc = cfg->svc;
	const struct bt_gatt_attr *out = NULL;

	for (size_t i = 0; i < svc->attr_count; i++) {
		const struct bt_gatt_attr *attr = &svc->attrs[i];

		if (attr->read == bt_gatt_attr_read_chrc) {
			out = attr;
		} else if (attr->read == read_report) {
			const struct hids_report *data = attr->user_data;

			if (data->report.id == report_id &&
			    data->report.type == HIDS_INPUT) {
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
	struct bt_conn *conn;
	const struct bt_gatt_attr *attr;
	int ret;
	uint8_t report_id;
	uint8_t buf[HOG_REPORT_BUF_SIZE];
	int size;

	if (data->hog_busy) {
		return;
	}

	size = hid_get_report(cfg->hid_dev, dev, &report_id, buf, sizeof(buf));
	if (size == -EAGAIN) {
		return;
	} else if (size < 0) {
		LOG_ERR("get_report error: %d", size);
		return;
	}

	if ((data->notify_mask & BIT(report_id)) == 0x00) {
		return;
	}

	if (data->suspended) {
		return;
	}

	data->hog_busy = true;

	attr = hog_find_attr(dev, report_id);
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

	if (data->peer_set) {
		conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &data->peer);
		if (conn == NULL) {
			LOG_WRN("cannot find a connection for peer");
		}
	} else {
		conn = NULL;
	}

	ret = bt_gatt_notify_cb(conn, &params);
	if (ret) {
		LOG_WRN("bt_gatt_notify_cb failed: %d", ret);
		data->hog_busy = false;
	}

	if (conn != NULL) {
		bt_conn_unref(conn);
	}
}

static int ble_hog_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	struct ble_hog_data *data = dev->data;

	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
		data->suspended = true;
		break;
	case PM_DEVICE_ACTION_RESUME:
		data->suspended = false;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int ble_hog_init(const struct device *dev)
{
	return 0;
}

DEVICE_API(hid_output, ble_hog_api) = {
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
	PM_DEVICE_DT_INST_DEFINE(n, ble_hog_pm_action);			\
									\
	static struct ble_hog_data ble_hog_data_##n;			\
									\
DEVICE_DT_INST_DEFINE(n, ble_hog_init, PM_DEVICE_DT_INST_GET(n),	\
		      &ble_hog_data_##n, &ble_hog_cfg_##n,		\
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,	\
		      &ble_hog_api);

DT_INST_FOREACH_STATUS_OKAY(BLE_HOG_INIT)
