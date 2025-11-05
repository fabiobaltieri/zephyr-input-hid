#include <zephyr/drivers/flash/stm32_flash_api_extensions.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(opt_bytes, LOG_LEVEL_INF);

static int opt_byte_set(void)
{
	int ret;
	static const struct device *flash = DEVICE_DT_GET(DT_NODELABEL(flash));
	uint32_t val;

	ret = flash_ex_op(flash, FLASH_STM32_EX_OP_OPTB_READ, 0, &val);
	if (ret < 0) {
		LOG_ERR("flash_ex_op error: %d", ret);
		return ret;
	}

	LOG_INF("OPTB=%08x", val);

	if (val & FLASH_OPTR_nBOOT_SEL_Msk) {
		val &= ~FLASH_OPTR_nBOOT_SEL_Msk;

		LOG_INF("Setting OPTB=%08x", val);

		ret = flash_ex_op(flash, FLASH_STM32_EX_OP_OPTB_WRITE, val, NULL);
		if (ret < 0) {
			LOG_ERR("flash_ex_op error: %d", ret);
			return ret;
		}
	}

	return 0;
}

SYS_INIT(opt_byte_set, APPLICATION, 1);
