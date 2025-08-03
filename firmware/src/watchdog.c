#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(watchdog, LOG_LEVEL_INF);

#if DT_NODE_EXISTS(DT_NODELABEL(wdt))

#define WDT_FEED_SECS 1

static const struct device *wdt = DEVICE_DT_GET(DT_NODELABEL(wdt));

static int wdt_channel_id;

static void wdt_feed_handler(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(wdt_dwork, wdt_feed_handler);

static void wdt_feed_handler(struct k_work *work)
{
	wdt_feed(wdt, wdt_channel_id);

	k_work_schedule(&wdt_dwork, K_SECONDS(WDT_FEED_SECS));
}

static int wdt_init(void)
{
	int ret;
	struct wdt_timeout_cfg wdt_config = {
		.flags = WDT_FLAG_RESET_SOC,
		.window.min = 0,
		.window.max = 5000,
	};

	if (!device_is_ready(wdt)) {
		LOG_ERR("wdt device is not ready");
		return -ENODEV;
	}

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id < 0) {
		LOG_ERR("watchdog install error: %d", wdt_channel_id);
		return 0;
	}

	ret = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (ret < 0) {
		LOG_ERR("watchdog setup error: %d", ret);
		return 0;
	}

	wdt_feed_handler(&wdt_dwork.work);

	return 0;
}
SYS_INIT(wdt_init, APPLICATION, 91);

#endif
