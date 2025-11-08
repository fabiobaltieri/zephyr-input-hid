#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(usb_board, LOG_LEVEL_INF);

extern void jump_to_bootloader(void);

#define USB_REQ_BOOTLOADER 0xfffe
#define USB_REQ_REBOOT 0xffff

static int to_host_cb(const struct usbd_context *const ctx,
		      const struct usb_setup_packet *const setup,
		      struct net_buf *const buf)
{
	return 0;
}

static int to_dev_cb(const struct usbd_context *const ctx,
		     const struct usb_setup_packet *const setup,
		     const struct net_buf *const buf)
{
	switch (setup->wIndex) {
	case USB_REQ_BOOTLOADER:
		jump_to_bootloader();
		break;
	case USB_REQ_REBOOT:
		sys_reboot(SYS_REBOOT_COLD);;
		break;
	default:
		break;
	}

	return 0;
}

USBD_VREQUEST_DEFINE(vnd_vreq, 0, to_host_cb, to_dev_cb);

static int usb_board_init(void)
{
	int err;

	STRUCT_SECTION_FOREACH(usbd_context, ctx) {
		err = usbd_device_register_vreq(ctx, &vnd_vreq);
		if (err) {
			LOG_ERR("Failed to register vreq");
			return err;
		}
	}

	return 0;
}

SYS_INIT(usb_board_init, APPLICATION, 0);
