#define DT_DRV_COMPAT hid

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "hid.h"

LOG_MODULE_REGISTER(hid, LOG_LEVEL_INF);

#define CACHE_SIZE 32
#define CACHE_ENTRIES 4

struct hid_report_cache {
	const struct device *input_dev;
	const struct device *output_dev;
	uint8_t report_id;
	uint8_t data[CACHE_SIZE];
	uint8_t size;
	bool updated;
};

struct hid_config {
	const uint8_t *report_map;
	uint16_t report_map_len;
	const struct device **output_dev;
	uint8_t output_dev_count;
};

struct hid_data {
	struct hid_report_cache cache[CACHE_ENTRIES];
	struct k_sem lock;
};

const uint8_t *hid_report(const struct device *dev)
{
	const struct hid_config *cfg = dev->config;

	return cfg->report_map;
}

uint16_t hid_report_len(const struct device *dev)
{
	const struct hid_config *cfg = dev->config;

	return cfg->report_map_len;
}

static struct hid_report_cache *find_cache_idx(const struct device *dev,
					       const struct device *input_dev,
					       const struct device *output_dev,
					       uint8_t input_id)
{
	struct hid_data *data = dev->data;
	uint8_t i;

	for (i = 0; i < CACHE_ENTRIES; i++) {
		struct hid_report_cache *entry = &data->cache[i];

		if (entry->input_dev == input_dev &&
		    entry->output_dev == output_dev &&
		    entry->report_id == input_id) {
			return entry;
		} else if (entry->input_dev == NULL &&
			   entry->output_dev == NULL &&
			   entry->report_id == 0) {
			LOG_INF("report cache init: %s %s %d: %d",
				input_dev->name, output_dev->name,
				input_id, i);
			entry->input_dev = input_dev;
			entry->output_dev = output_dev;
			entry->report_id = input_id;
			return entry;
		}
	}

	LOG_ERR("report cache full (%s %s %d)",
		input_dev->name,
		output_dev->name,
		input_id);

	return NULL;
}

static void update_buffers(const struct device *dev,
			   const struct device *input_dev,
			   uint8_t input_id,
			   update_buffers_cb_t cb,
			   void *user_data)
{
	const struct hid_config *cfg = dev->config;
	struct hid_data *data = dev->data;
	int size;

	for (uint8_t i = 0; i < cfg->output_dev_count; i++) {
		const struct device *output_dev = cfg->output_dev[i];
		struct hid_report_cache *entry;

		k_sem_take(&data->lock, K_FOREVER);

		entry = find_cache_idx(dev, input_dev, output_dev, input_id);
		if (entry == NULL) {
			continue;
		}

		size = cb(input_dev, entry->data, CACHE_SIZE, user_data);
		if (size < 0) {
			LOG_ERR("%s cb error: %d", input_dev->name, size);
		}

		entry->size = size;

		// TODO: only update on changes
		entry->updated = true;

		k_sem_give(&data->lock);

		hid_output_notify(output_dev);
	}
}

static bool has_updates(const struct device *dev,
			const struct device *output_dev)
{
	struct hid_data *data = dev->data;
	bool out = false;

	k_sem_take(&data->lock, K_FOREVER);

	for (uint8_t i = 0; i < CACHE_ENTRIES; i++) {
		struct hid_report_cache *entry = &data->cache[i];

		if (entry->output_dev != output_dev || !entry->updated) {
			continue;
		}

		out = true;
		break;
	}

	k_sem_give(&data->lock);

	return out;
}

static int get_report(const struct device *dev,
		      const struct device *output_dev,
		      uint8_t *report_id,
		      uint8_t *buf,
		      uint8_t size)
{
	struct hid_data *data = dev->data;
	int out = -EINVAL;

	k_sem_take(&data->lock, K_FOREVER);

	for (uint8_t i = 0; i < CACHE_ENTRIES; i++) {
		struct hid_report_cache *entry = &data->cache[i];

		if (entry->output_dev != output_dev || !entry->updated) {
			continue;
		}

		if (size < entry->size) {
			LOG_ERR("buffer too small: %d < %d", size, entry->size);
			break;
		}

		memcpy(buf, entry->data, entry->size);
		*report_id = entry->report_id;
		out = entry->size;

		entry->updated = false;

		break;
	}

	k_sem_give(&data->lock);

	return out;
}

static int hid_init(const struct device *dev)
{
	struct hid_data *data = dev->data;

	k_sem_init(&data->lock, 1, 1);

	return 0;
}

static const struct hid_api hid_api = {
	.update_buffers = update_buffers,
	.has_updates = has_updates,
	.get_report = get_report,
};

#define HID_REPORT_BYTE(node_id, prop, idx) \
	DT_PROP_BY_IDX(node_id, prop, idx),

#define HID_REPORT_DEVICE(node_id) \
	DT_FOREACH_PROP_ELEM(node_id, report, HID_REPORT_BYTE)

#define OUPUT_DEVICE_GET(node_id, prop, idx) \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define HID_INIT(n)								\
	static const uint8_t hid_report_map_##n[] = {				\
		DT_INST_FOREACH_CHILD(n, HID_REPORT_DEVICE)			\
	};									\
										\
	static const struct device *hid_output_dev_##n[] = {			\
		DT_INST_FOREACH_PROP_ELEM(n, output, OUPUT_DEVICE_GET)		\
	};									\
										\
	static const struct hid_config hid_cfg_##n = {				\
		.report_map = hid_report_map_##n,				\
		.report_map_len = sizeof(hid_report_map_##n),			\
		.output_dev = hid_output_dev_##n,				\
		.output_dev_count = ARRAY_SIZE(hid_output_dev_##n),		\
	};									\
										\
	static struct hid_data hid_data_##n;					\
										\
	DEVICE_DT_INST_DEFINE(n, hid_init, NULL,				\
			      &hid_data_##n, &hid_cfg_##n,			\
			      POST_KERNEL, 55,					\
			      &hid_api);

DT_INST_FOREACH_STATUS_OKAY(HID_INIT)
