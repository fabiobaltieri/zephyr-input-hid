#define DT_DRV_COMPAT xinput

#include <zephyr/device.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usbd.h>

LOG_MODULE_REGISTER(xinput, LOG_LEVEL_INF);

struct xinput_report {
	uint8_t report_id;
	uint8_t report_size;
	uint8_t buttons_0;
	uint8_t buttons_1;
	uint8_t lz;
	uint8_t rz;
	int16_t lx;
	int16_t ly;
	int16_t rx;
	int16_t ry;
	uint8_t reserved[6];
} __packed;

struct xinput_desc {
	struct usb_if_descriptor if0;
	char undocumented[16];
	struct usb_ep_descriptor if0_in_ep;
	struct usb_ep_descriptor if0_in_ep_hs;
	struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_out_ep_hs;
};

struct xinput_config {
	struct net_buf_pool *pool_in;
	struct net_buf_pool *pool_out;
	struct usbd_class_data *c_data;
	struct xinput_desc *desc;
	const struct usb_desc_header **fs_desc;
	const struct usb_desc_header **hs_desc;
};

struct xinput_data {
	const struct device *dev;
	struct k_work out_work;
};

#define XINPUT_BUF_COUNT 4

#define INTERVAL_IN_US 4000
#define INTERVAL_OUT_US 8000

#define XINPUT_DESC_INIT()						\
{									\
	.if0 = {							\
		.bLength = sizeof(struct usb_if_descriptor),		\
		.bDescriptorType = USB_DESC_INTERFACE,			\
		.bInterfaceNumber = 0,					\
		.bAlternateSetting = 0,					\
		.bNumEndpoints = 2,					\
		.bInterfaceClass = USB_BCC_VENDOR,			\
		.bInterfaceSubClass = 0x5d,				\
		.bInterfaceProtocol = 1,				\
		.iInterface = 0,					\
	},								\
	.undocumented = {						\
		0x10, 0x21, 0x10, 0x01, 0x01, 0x24, 0x81, 0x14,		\
		0x03, 0x00, 0x03, 0x13, 0x02, 0x00, 0x03, 0x00,		\
	},								\
	.if0_in_ep = {							\
		.bLength = sizeof(struct usb_ep_descriptor),		\
		.bDescriptorType = USB_DESC_ENDPOINT,			\
		.bEndpointAddress = 0x81,				\
		.bmAttributes = USB_EP_TYPE_INTERRUPT,			\
		.wMaxPacketSize = sys_cpu_to_le16(32U),			\
		.bInterval = USB_FS_INT_EP_INTERVAL(INTERVAL_IN_US),	\
	},								\
	.if0_in_ep_hs = {						\
		.bLength = sizeof(struct usb_ep_descriptor),		\
		.bDescriptorType = USB_DESC_ENDPOINT,			\
		.bEndpointAddress = 0x81,				\
		.bmAttributes = USB_EP_TYPE_INTERRUPT,			\
		.wMaxPacketSize = sys_cpu_to_le16(32U),			\
		.bInterval = USB_HS_INT_EP_INTERVAL(INTERVAL_IN_US),	\
	},								\
	.if0_out_ep = {							\
		.bLength = sizeof(struct usb_ep_descriptor),		\
		.bDescriptorType = USB_DESC_ENDPOINT,			\
		.bEndpointAddress = 0x01,				\
		.bmAttributes = USB_EP_TYPE_INTERRUPT,			\
		.wMaxPacketSize = sys_cpu_to_le16(32U),			\
		.bInterval = USB_FS_INT_EP_INTERVAL(INTERVAL_OUT_US),	\
	},								\
	.if0_out_ep_hs = {						\
		.bLength = sizeof(struct usb_ep_descriptor),		\
		.bDescriptorType = USB_DESC_ENDPOINT,			\
		.bEndpointAddress = 0x01,				\
		.bmAttributes = USB_EP_TYPE_INTERRUPT,			\
		.wMaxPacketSize = sys_cpu_to_le16(32U),			\
		.bInterval = USB_HS_INT_EP_INTERVAL(INTERVAL_OUT_US),	\
	},								\
};

#define XINPUT_FS_DESC_HEADER(desc)			\
{							\
	(struct usb_desc_header *)&desc.if0,		\
	(struct usb_desc_header *)&desc.undocumented,	\
	(struct usb_desc_header *)&desc.if0_out_ep,	\
	(struct usb_desc_header *)&desc.if0_in_ep,	\
	NULL,						\
};

#define XINPUT_HS_DESC_HEADER(desc)			\
{							\
	(struct usb_desc_header *)&desc.if0,		\
	(struct usb_desc_header *)&desc.undocumented,	\
	(struct usb_desc_header *)&desc.if0_out_ep_hs,	\
	(struct usb_desc_header *)&desc.if0_in_ep_hs,	\
	NULL,						\
};

static void xinput_update(struct usbd_class_data *c_data,
			  uint8_t iface, uint8_t alternate)
{
	LOG_INF("%s\n", __func__);
}

static int xinput_control_to_host(struct usbd_class_data *c_data,
				  const struct usb_setup_packet *const setup,
				  struct net_buf *const buf)
{
	if (setup->bRequest == 0x01 &&
	    setup->wValue == 0x100 &&
	    setup->wIndex == 0x00) {
		uint8_t dummy[20];
		memset(dummy, 0, sizeof(dummy));
		net_buf_add_mem(buf, dummy,
				MIN(sizeof(dummy), setup->wLength));
	}

	LOG_INF("%s\n", __func__);
	return 0;
}

static int xinput_control_to_dev(struct usbd_class_data *c_data,
				 const struct usb_setup_packet *const setup,
				 const struct net_buf *const buf)
{
	LOG_INF("%s\n", __func__);
	return 0;
}

static int xinput_request_handler(struct usbd_class_data *c_data,
				  struct net_buf *buf, int err)
{
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	const struct device *dev = usbd_class_get_private(c_data);
	const struct xinput_config *cfg = dev->config;
	struct xinput_data *data = dev->data;
	struct udc_buf_info *bi;

	bi = net_buf_user_data(buf);

	if (bi->ep == cfg->desc->if0_out_ep.bEndpointAddress) {
		LOG_HEXDUMP_INF(buf->data, buf->len, "data");

		k_work_submit(&data->out_work);
	} else if (bi->ep == cfg->desc->if0_in_ep.bEndpointAddress) {
		LOG_DBG("ep %02x done", bi->ep);
	} else {
		LOG_INF("%p -> ep 0x%02x, len %u, err %d", c_data, bi->ep, buf->len, err);
	}

	return usbd_ep_buf_free(uds_ctx, buf);
}

static void *xinput_get_desc(struct usbd_class_data *const c_data,
			     const enum usbd_speed speed)
{
	const struct device *dev = usbd_class_get_private(c_data);
	const struct xinput_config *cfg = dev->config;

	if (speed == USBD_SPEED_HS) {
		return cfg->hs_desc;
	}

	return cfg->fs_desc;
}

static void xinput_enable(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct xinput_data *data = dev->data;

	k_work_submit(&data->out_work);

	LOG_INF("%s\n", __func__);
}

static void xinput_disable(struct usbd_class_data *const c_data)
{
	LOG_INF("%s\n", __func__);
}

static int xinput_usb_init(struct usbd_class_data *c_data)
{
	LOG_INF("%s\n", __func__);
	return 0;
}

struct usbd_class_api xinput_api = {
	.update = xinput_update,
	.enable = xinput_enable,
	.disable = xinput_disable,
	.control_to_host = xinput_control_to_host,
	.control_to_dev = xinput_control_to_dev,
	.request = xinput_request_handler,
	.get_desc = xinput_get_desc,
	.init = xinput_usb_init,
};

static const struct usbd_cctx_vendor_req xinput_vregs = USBD_VENDOR_REQ(0x01);

static void xinput_send_report(const struct device *dev,
			       struct xinput_report *report)
{
	const struct xinput_config *cfg = dev->config;
	struct net_buf *buf = NULL;
	struct udc_buf_info *bi;
	int ret;

	buf = net_buf_alloc_with_data(cfg->pool_in,
				      report, sizeof(*report), K_NO_WAIT);
	if (!buf) {
		return;
	}

	bi = udc_get_buf_info(buf);
	memset(bi, 0, sizeof(struct udc_buf_info));

	bi->ep = cfg->desc->if0_in_ep.bEndpointAddress;

	ret = usbd_ep_enqueue(cfg->c_data, buf);
	if (ret) {
		net_buf_unref(buf);
		return;
	}
}

static void xinput_out_handler(struct k_work *work)
{
	struct xinput_data *data = CONTAINER_OF(work, struct xinput_data, out_work);
	const struct device *dev = data->dev;
	const struct xinput_config *cfg = dev->config;
	struct usbd_class_data *c_data = cfg->c_data;

        struct net_buf *buf;
        struct udc_buf_info *bi;

	buf = net_buf_alloc(cfg->pool_out, K_NO_WAIT);
        if (!buf) {
                return;
        }

	bi = udc_get_buf_info(buf);
        memset(bi, 0, sizeof(struct udc_buf_info));

        bi->ep = cfg->desc->if0_out_ep.bEndpointAddress;

	if (usbd_ep_enqueue(c_data, buf)) {
		net_buf_unref(buf);
		LOG_ERR("Failed to enqueue buffer");
		return;
	}
}

static void xinput_cb(const struct device *dev, struct input_event *evt)
{
	struct xinput_report report;

	memset(&report, 0, sizeof(report));
	report.report_id = 0;
	report.report_size = sizeof(report);

	switch (evt->code) {
	case INPUT_BTN_SOUTH:
		WRITE_BIT(report.buttons_1, 4, evt->value);
		break;
	case INPUT_BTN_EAST:
		WRITE_BIT(report.buttons_1, 5, evt->value);
		break;
	case INPUT_BTN_NORTH:
		WRITE_BIT(report.buttons_1, 6, evt->value);
		break;
	case INPUT_BTN_WEST:
		WRITE_BIT(report.buttons_1, 7, evt->value);
		break;
	}

	xinput_send_report(dev, &report);
}

static int xinput_init(const struct device *dev)
{
	struct xinput_data *data = dev->data;

	data->dev = dev;

	k_work_init(&data->out_work, xinput_out_handler);

	return 0;
}

#define XINPUT_CB(node_id, prop, idx, n)						\
	static void xinput_cb_##n##_##idx(struct input_event *evt)			\
	{										\
		xinput_cb(DEVICE_DT_GET(node_id), evt);					\
	}										\
	INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),	\
			      xinput_cb_##n##_##idx);

#define XINPUT_DEFINE(n)								\
	DT_INST_FOREACH_PROP_ELEM_VARGS(n, input, XINPUT_CB, n)				\
											\
	USBD_DEFINE_CLASS(xinput_##n, &xinput_api,					\
			  (void *)DEVICE_DT_GET(DT_DRV_INST(n)),			\
			  &xinput_vregs);						\
											\
	NET_BUF_POOL_DEFINE(xinput_buf_pool_in_##n, XINPUT_BUF_COUNT, 0,		\
		    sizeof(struct udc_buf_info), NULL);					\
	NET_BUF_POOL_DEFINE(xinput_buf_pool_out_##n, XINPUT_BUF_COUNT, 32,		\
		    sizeof(struct udc_buf_info), NULL);					\
											\
	struct xinput_desc xinput_desc_##n = XINPUT_DESC_INIT();			\
	const static struct usb_desc_header *xinput_fs_desc_##n[] =			\
		XINPUT_FS_DESC_HEADER(xinput_desc_##n);					\
	const static struct usb_desc_header *xinput_hs_desc_##n[] =			\
		XINPUT_HS_DESC_HEADER(xinput_desc_##n);					\
											\
	static const struct xinput_config xinput_cfg_##n = {				\
		.pool_in = &xinput_buf_pool_in_##n,					\
		.pool_out = &xinput_buf_pool_out_##n,					\
		.c_data = &xinput_##n,							\
		.desc = &xinput_desc_##n,						\
		.fs_desc = xinput_fs_desc_##n,						\
		.hs_desc = xinput_hs_desc_##n,						\
	};										\
											\
	static struct xinput_data xinput_data_##n;					\
											\
	DEVICE_DT_INST_DEFINE(n, xinput_init, NULL,					\
			      &xinput_data_##n, &xinput_cfg_##n,			\
			      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(XINPUT_DEFINE);
