#include <stdint.h>
#include <zephyr/device.h>

const uint8_t *hid_report(const struct device *dev);

uint16_t hid_report_len(const struct device *dev);

typedef int (*update_buffers_cb_t)(const struct device *dev,
				   uint8_t *buf, uint8_t len,
				   void *user_data);

void hid_update_buffers(const struct device *dev,
			const struct device *input_dev,
			uint8_t input_id,
			update_buffers_cb_t cb,
			void *user_data);

int hid_get_report(const struct device *dev,
		   const struct device *output_dev,
		   uint8_t *report_id, uint8_t *buf, uint8_t size);

int hid_get_report_id(const struct device *dev,
		      const struct device *output_dev,
		      uint8_t report_id, uint8_t *buf, uint8_t size);

void hid_out_report(const struct device *dev,
		    uint8_t report_id, const uint8_t *buf, uint8_t len);

int hid_set_feature(const struct device *dev,
		    uint8_t report_id, const uint8_t *buf, uint8_t len);

__subsystem struct hid_input_api {
	int (*clear_rel)(const struct device *dev,
			 uint8_t *buf, uint8_t len);
	int (*out_report)(const struct device *dev,
			  uint8_t report_id, const uint8_t *buf, uint8_t len);
	int (*set_feature)(const struct device *dev,
			   uint8_t report_id, const uint8_t *buf, uint8_t len);
};

static inline int hid_has_clear_rel(const struct device *dev)
{
	const struct hid_input_api *api = (const struct hid_input_api *)dev->api;

	return api != NULL && api->clear_rel != NULL;
}

static inline int hid_clear_rel(const struct device *dev,
				uint8_t *buf, uint8_t len)
{
	const struct hid_input_api *api = (const struct hid_input_api *)dev->api;

	if (api == NULL || api->clear_rel == NULL) {
		return -ENOSYS;
	};

	return api->clear_rel(dev, buf, len);
}

static inline int hid_input_out_report(const struct device *dev,
				       uint8_t report_id, const uint8_t *buf, uint8_t len)
{
	const struct hid_input_api *api = (const struct hid_input_api *)dev->api;

	if (api == NULL || api->out_report == NULL) {
		return -ENOSYS;
	};

	return api->out_report(dev, report_id, buf, len);
}

static inline int hid_input_set_feature(const struct device *dev,
					uint8_t report_id, const uint8_t *buf, uint8_t len)
{
	const struct hid_input_api *api = (const struct hid_input_api *)dev->api;

	if (api == NULL || api->set_feature == NULL) {
		return -ENOSYS;
	};

	return api->set_feature(dev, report_id, buf, len);
}

__subsystem struct hid_output_api {
	void (*notify)(const struct device *dev);
};

static inline void hid_output_notify(const struct device *dev)
{
	const struct hid_output_api *api = (const struct hid_output_api *)dev->api;

	api->notify(dev);
}
